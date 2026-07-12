#include <iostream>
#include <sstream>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "sdkconfig.h"
//#include "esp_pm.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "esp_flash.h"
#include "esp_partition.h"

#include "chars.h"
#include "pwm.h"


//#include "tinyusb_default_config.h"

//#define FLASH_TARGET_OFFSET (2 * 1024 * 1024 - FLASH_SECTOR_SIZE)

//const tinyusb_config_t tusb_cfg;

const uint8_t* contents;
uint32_t saved_interrupts;


static const char* TAG = "RAW_FLASH";
#define SECTOR_SIZE 4096
const esp_partition_t* partition = esp_partition_find_first(
    ESP_PARTITION_TYPE_DATA,
    ESP_PARTITION_SUBTYPE_ANY,
    "storage"
);


int dit_state;
int dah_state;

int last = -1;
int curent = -1;
int next = -1;

int speed = 22;
int tone = 440;

int basetime = 1200 / speed;

std::string str;

std::vector<int> elements;
uint32_t lastChar = 0;

bool recordMode = false;
std::vector<int> recordArr;
bool hasSpace = true;
bool recordLock = false;
#ifdef KEYBOARD_KEYER
bool usbState = false;
uint8_t keycode[6] = { 0 };
#endif

esp_err_t err;


#ifdef MIDI_KEYER
uint8_t msg[4];
#endif
std::string decodeChar(std::vector<int> elements) {
    std::stringstream stream;
    bool match = true;
    for (int i = 0; i < chars.size(); i++) {
        if (chars.at(i).size() == elements.size()) {
            for (int x = 0; x < elements.size(); x++) {
                if (elements.at(x) != chars.at(i).at(x)) {
                    match = false;
                    break;
                }
            }
        } else {
            match = false;
        }
        if (match) {
            stream << strs.at(i);
            return stream.str();
        }
        match = true;
    }
    return "_";
}

#ifdef KEYBOARD_KEYER
void sendKey(std::string s) {
    if (tud_suspended()) {
        tud_remote_wakeup();
    }
    uint8_t conv_table[128][2] = { HID_ASCII_TO_KEYCODE };
    keycode[0] = conv_table[(int)*s.c_str()][1];

}
#endif

void key(bool x) {
    if (x) {
        setPWMEnable(true);
        #ifdef MIDI_KEYER
        msg[0] = 0x09; // Note On - Channel 1
        msg[1] = 0x90;   // Note Number
        msg[2] = 1;
        msg[3] = 0x7F;  // Velocity
        tud_midi_n_stream_write(0, 0, msg, 4);
        #endif
    } else {
        setPWMEnable(false);
        #ifdef MIDI_KEYER
        msg[0] = 0x08; // Note On - Channel 1
        msg[1] = 0x80;   // Note Number
        msg[2] = 1;
        msg[3] = 0x0;  // Velocity
        tud_midi_n_stream_write(0, 0, msg, 4);
        #endif
    }
    #ifdef MIDI_KEYER
    tud_task();
    #endif
}

void coolDown(void* arg) {
    curent = -1;
}

void turnOff(void* arg) {
    key(false);
    const esp_timer_create_args_t timer_args = {
        .callback = &coolDown,
        .arg = arg,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "coolDown",
        .skip_unhandled_events = false
    };

    esp_timer_handle_t timer_handle = NULL;
    esp_timer_create(&timer_args, &timer_handle);
    esp_timer_start_once(timer_handle, basetime * 1000);

    lastChar = esp_timer_get_time() / 1000;
    elements.push_back(*((int*)arg));
    if (recordMode) {
        recordArr.push_back(*((int*)arg));
    }

}

void doDit() {
    key(true);
    const esp_timer_create_args_t timer_args = {
        .callback = &turnOff,
        .arg = (void*)&dit,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "dit",
        .skip_unhandled_events = false
    };

    esp_timer_handle_t timer_handle = NULL;
    esp_timer_create(&timer_args, &timer_handle);
    esp_timer_start_once(timer_handle, basetime * 1000);
    hasSpace = false;
    curent = dit;
    last = dit;
}

void doDah() {
    key(true);
    const esp_timer_create_args_t timer_args = {
    .callback = &turnOff,
    .arg = (void*)&dah,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "dah",
    .skip_unhandled_events = false
    };

    esp_timer_handle_t timer_handle = NULL;
    esp_timer_create(&timer_args, &timer_handle);
    esp_timer_start_once(timer_handle, basetime * 1000 * 3);
    hasSpace = false;
    curent = dah;
    last = dah;
}

extern "C" void app_main() {
    initPWM(tone, pwmPin);


    key(true);
    vTaskDelay(pdMS_TO_TICKS(100));
    key(false);

    #ifdef KEYBOARD_KEYER

    // init device stack on configured roothub port
    const tusb_rhport_init_t rh_init = {
      .role = TUSB_ROLE_DEVICE,
      .speed = TUD_OPT_HIGH_SPEED ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL
    };
    TU_ASSERT(tud_rhport_init(BOARD_TUD_RHPORT, &rh_init));
    #endif

    gpio_set_direction(ditPin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(ditPin, GPIO_PULLUP_ONLY);

    gpio_set_direction(dahPin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(dahPin, GPIO_PULLUP_ONLY);


    gpio_set_direction(recordPin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(recordPin, GPIO_PULLUP_ONLY);

    gpio_set_direction(playPin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(playPin, GPIO_PULLUP_ONLY);

    while (true) {
        //tud_task();
        dit_state = !gpio_get_level(ditPin);
        dah_state = !gpio_get_level(dahPin);
        if (dah_state && dit_state) {
            //printf("both\n");
            next = 3;
        } else if (dit_state && !dah_state) {
            //printf("dit\n");
            if (curent == dah || curent == -1) {
                next = dit;
            }
        } else if (dah_state && !dit_state) {
            //printf("dah\n");

            if (curent == dit || curent == -1) {
                next = dah;
            }
        }

        if (curent == -1) {

            if (next == dit) {
                doDit();
                next = -1;
            } else if (next == dah) {
                doDah();
                next = -1;
            } else if (next == 3) {
                if (last == dit) {
                    doDah();
                } else if (last == dah)
                {
                    doDit();
                }
                next = -1;
            }
        }
        if (esp_timer_get_time() / 100 - lastChar > basetime * 7 && !hasSpace && curent == -1) {
            printf(" ");
            #ifdef KEYBOARD_KEYER
            sendKey(" ");
            #endif

            hasSpace = true;
            if (recordMode) {
                recordArr.pop_back();
                recordArr.push_back(space);
            }
        } else if (elements.size() > 0 && esp_timer_get_time() / 100 - lastChar > basetime * 2.8 && curent == -1) {
            printf(decodeChar(elements).c_str());
            #ifdef KEYBOARD_KEYER
            sendKey(decodeChar(elements));
            #endif
            elements.clear();
            if (recordMode && recordArr.size() > 0) {
                recordArr.push_back(gap);
            }
        }

        if (!gpio_get_level(playPin)) {
            //contents = (const uint8_t*)(XIP_BASE + FLASH_TARGET_OFFSET);
            if (partition == NULL) {
                ESP_LOGE(TAG, "Partition 'storage' not found!");
                return;
            }

            for (int i = 0; i < FLASH_PAGE_SIZE;i++) {
                if (contents[i] == end) {
                    hasSpace = false;
                    break;
                } else if (contents[i] == dit) {
                    key(true);
                    sleep_ms(basetime);
                    key(false);
                    sleep_ms(basetime);
                    elements.push_back(dit);
                } else if (contents[i] == dah) {
                    key(true);
                    sleep_ms(basetime * 3);
                    key(false);
                    sleep_ms(basetime);
                    elements.push_back(dah);
                } else if (contents[i] == gap) {
                    printf(decodeChar(elements).c_str());
                    elements.clear();
                    sleep_ms(basetime * 3);
                } else if (contents[i] == space) {
                    printf(decodeChar(elements).c_str());
                    elements.clear();
                    printf(" ");
                    sleep_ms(basetime * 7);
                }
            }
        }

        if (gpio_get_level(recordPin)) {
            recordLock = false;
        }
        if (!gpio_get_level(recordPin) && !recordMode && !recordLock) {
            recordLock = true;
            recordMode = true;
            printf(" start-- ");
        } else if (!gpio_get_level(recordPin) && recordMode && !recordLock) {
            recordLock = true;
            recordMode = false;
            printf(" --stop ");
            if (recordArr.size() != 0) {
                if (partition == NULL) {
                    ESP_LOGE(TAG, "Partition 'storage' not found!");
                    return;
                }

                recordArr.pop_back();
                recordArr.push_back(end);

                uint8_t write_buf[recordArr.size()];
                for (int i = 0; i < recordArr.size(); i++) {
                    write_buf[i] = 4;
                }

                for (int i = 0; i < recordArr.size(); i++) {
                    write_buf[i] = recordArr.at(i);
                }

                err = esp_partition_write(partition, 0x0000, write_buf, sizeof(write_buf));
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Write failed: %s", esp_err_to_name(err));
                    return;
                }
                recordArr.clear();
            }
        }
        #ifdef KEYBOARD_KEYER
        if (tud_hid_ready()) {
            if (!usbState && keycode[0] > 0) {
                tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
                usbState = true;
            } else if (usbState) {

                keycode[0] = 0;
                tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
                usbState = false;
            }
        }
        #endif
    }
}


// int initTinyUSB() {
//     tusb_cfg = TINYUSB_DEFAULT_CONFIG();
//     tinyusb_driver_install(&tusb_cfg);
//     return true;
// }