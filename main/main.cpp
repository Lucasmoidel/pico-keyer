#include <iostream>
#include <sstream>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "sdkconfig.h"
//#include "esp_pm.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "esp_flash.h"
#include "esp_partition.h"

#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid_device.h"

#include "chars.h"
#include "pwm.h"


static const char* TAG = "RAW_FLASH";
#define SECTOR_SIZE 4096
const esp_partition_t* partition;


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

const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(HID_ITF_PROTOCOL_MOUSE))
};

const char* hid_string_descriptor[5] = { (char[]) { 0x09, 0x04 },"TinyUSB","TinyUSB Device","123456","Example HID interface", };

static const uint8_t hid_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN), TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor), 0x81, 16, 10),
};

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {}
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) { return 0; }
uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance) { return hid_report_descriptor; }
#endif

esp_err_t err;

int initTinyUSB();
void setTimer(esp_timer_cb_t func, void* arg, uint64_t time);

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
    setTimer(&coolDown, arg, basetime);

    lastChar = esp_timer_get_time() / 1000;
    elements.push_back(*((int*)arg));
    if (recordMode) {
        recordArr.push_back(*((int*)arg));
    }

}

void doDit() {
    key(true);
    setTimer(&turnOff, (void*)&dit, basetime);
    hasSpace = false;
    curent = dit;
    last = dit;
}

void doDah() {
    key(true);
    setTimer(&turnOff, (void*)&dah, basetime * 3);

    hasSpace = false;
    curent = dah;
    last = dah;
}

extern "C" void app_main() {

    partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        "storage"
    );
    if (partition == NULL) {
        ESP_LOGE(TAG, "Partition 'storage' not found!");
        return;
    }
    initPWM(tone, pwmPin);


    key(true);
    vTaskDelay(pdMS_TO_TICKS(100));
    key(false);

    #ifdef KEYBOARD_KEYER

    initTinyUSB();
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
        if (esp_timer_get_time() / 1000 - lastChar > basetime * 7 && !hasSpace && curent == -1) {
            printf(" ");
            #ifdef KEYBOARD_KEYER
            sendKey(" ");
            #endif

            hasSpace = true;
            if (recordMode) {
                recordArr.pop_back();
                recordArr.push_back(space);
            }
        } else if (elements.size() > 0 && esp_timer_get_time() / 1000 - lastChar > basetime * 2.8 && curent == -1) {
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
            printf("\n Playing memory: ");
            fflush(stdout);
            fsync(fileno(stdout));
            uint8_t* readbuf = new uint8_t[SECTOR_SIZE];
            esp_partition_read(partition, 0x0000, readbuf, SECTOR_SIZE);

            for (int i = 0; i < SECTOR_SIZE - 1;i++) {
                if (readbuf[i] == end) {
                    hasSpace = false;
                    break;
                } else if (readbuf[i] == dit) {
                    key(true);
                    vTaskDelay(pdMS_TO_TICKS(basetime));
                    key(false);
                    vTaskDelay(pdMS_TO_TICKS(basetime));
                    elements.push_back(dit);
                } else if (readbuf[i] == dah) {
                    key(true);
                    vTaskDelay(pdMS_TO_TICKS(basetime * 3));
                    key(false);
                    vTaskDelay(pdMS_TO_TICKS(basetime));
                    elements.push_back(dah);
                } else if (readbuf[i] == gap) {
                    printf(decodeChar(elements).c_str());
                    #ifdef KEYBOARD_KEYER
                    sendKey(decodeChar(elements));
                    #endif
                    elements.clear();
                    vTaskDelay(pdMS_TO_TICKS(basetime * 3));
                } else if (readbuf[i] == space) {
                    printf(decodeChar(elements).c_str());
                    #ifdef KEYBOARD_KEYER
                    sendKey(" ");
                    #endif
                    elements.clear();
                    printf(" ");
                    vTaskDelay(pdMS_TO_TICKS(basetime * 7));

                }
                fflush(stdout);
                fsync(fileno(stdout));
            }
            delete[] readbuf;
            printf(" done\n");
        }

        if (gpio_get_level(recordPin)) {
            recordLock = false;
        }
        if (!gpio_get_level(recordPin) && !recordMode && !recordLock) {
            recordLock = true;
            recordMode = true;
            printf("\n start-- ");
            vTaskDelay(pdMS_TO_TICKS(500));

        } else if (!gpio_get_level(recordPin) && recordMode && !recordLock) {
            recordLock = true;
            recordMode = false;
            printf(" --stop \n");
            if (recordArr.size() != 0) {

                recordArr.pop_back();
                recordArr.push_back(end);


                uint8_t* write_buf = new uint8_t[SECTOR_SIZE];
                for (int i = 0; i < SECTOR_SIZE; i++) {
                    write_buf[i] = 4;
                }

                for (int i = 0; i < recordArr.size() && i < SECTOR_SIZE - 1; i++) {
                    write_buf[i] = recordArr.at(i);
                }
                esp_err_t err = esp_flash_erase_region(esp_flash_default_chip, partition->address, SECTOR_SIZE);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Erase failed: %s", esp_err_to_name(err));
                    return;
                }
                err = esp_partition_write(partition, 0x0000, write_buf, SECTOR_SIZE);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Write failed: %s", esp_err_to_name(err));
                    return;
                }
                recordArr.clear();
                delete[] write_buf;
            }
        }
        #ifdef KEYBOARD_KEYER
        if (tud_hid_ready()) {
            if (!usbState && keycode[0] > 0) {
                tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keycode);
                usbState = true;
            } else if (usbState) {

                keycode[0] = 0;
                tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keycode);
                usbState = false;
            }
        }
        #endif
        fflush(stdout);
        fsync(fileno(stdout));


    }
}


int initTinyUSB() {
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();

    tusb_cfg.descriptor.device = NULL;
    tusb_cfg.descriptor.full_speed_config = hid_configuration_descriptor;
    tusb_cfg.descriptor.string = hid_string_descriptor;
    tusb_cfg.descriptor.string_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]);
    #if (TUD_OPT_HIGH_SPEED)
    tusb_cfg.descriptor.high_speed_config = hid_configuration_descriptor;
    #endif // TUD_OPT_HIGH_SPEED

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB initialization DONE");
    return true;
}

void setTimer(esp_timer_cb_t func, void* arg, uint64_t time) {
    const esp_timer_create_args_t timer_args = {
        .callback = func,
        .arg = arg,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "timer",
        .skip_unhandled_events = false
    };

    esp_timer_handle_t timer_handle = NULL;
    esp_timer_create(&timer_args, &timer_handle);
    esp_timer_start_once(timer_handle, time * 1000);
}