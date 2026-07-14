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

#include "chars.h"
#include "pwm.h"
#include "usb_defs.h"


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

bool rotloc = false;

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
    uint8_t keycode[6] = { 0 };

    if (tud_suspended()) {
        tud_remote_wakeup();
    }
    if (s == " ") {
        keycode[0] = HID_KEY_SPACE;
    } else {
        uint8_t conv_table[128][2] = { HID_ASCII_TO_KEYCODE };
        keycode[0] = conv_table[(int)*s.c_str()][1];
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keycode);
    keycode[0] = 0;
    vTaskDelay(pdMS_TO_TICKS(10));
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keycode);
}
#endif

void key(bool x) {
    if (x) {
        setPWMEnable(true);
        #ifdef MIDI_KEYER
        msg[0] = 0x09; // Note On - Channel 1
        msg[1] = 0x90; // Note Number
        msg[2] = 1;
        msg[3] = 0x7F; // Velocity
        tud_midi_stream_write(0, msg, 4);
        #endif
    } else {
        setPWMEnable(false);
        #ifdef MIDI_KEYER
        msg[0] = 0x08; // Note Off - Channel 1
        msg[1] = 0x80; // Note Number
        msg[2] = 1;
        msg[3] = 0x0;  // Velocity
        tud_midi_stream_write(0, msg, 4);
        #endif
    }
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

    #ifdef USB_KEYER
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

    gpio_set_direction(rotA, GPIO_MODE_INPUT);
    gpio_set_pull_mode(rotA, GPIO_PULLUP_ONLY);

    gpio_set_direction(rotB, GPIO_MODE_INPUT);
    gpio_set_pull_mode(rotB, GPIO_PULLUP_ONLY);

    gpio_set_direction(recLedPin, GPIO_MODE_OUTPUT);
    gpio_set_level(recLedPin, 0);
    while (true) {
        //tud_task();
        dit_state = !gpio_get_level(ditPin);
        dah_state = !gpio_get_level(dahPin);
        if (dah_state && dit_state) {
            next = 3;
        } else if (dit_state && !dah_state) {
            if (curent == dah || curent == -1) {
                next = dit;
            }
        } else if (dah_state && !dit_state) {
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
        } else if (elements.size() > 0 && esp_timer_get_time() / 1000 - lastChar > basetime * 5 && curent == -1) {
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
            fflush(stdout);
            fsync(fileno(stdout));
            uint8_t* readbuf = new uint8_t[SECTOR_SIZE];
            esp_partition_read(partition, 0x0000, readbuf, SECTOR_SIZE);

            for (int i = 0; i < SECTOR_SIZE - 1;i++) {
                if (readbuf[i] == end) {
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
                    lastChar = esp_timer_get_time() / 1000;
                    printf(decodeChar(elements).c_str());
                    #ifdef KEYBOARD_KEYER
                    sendKey(decodeChar(elements));
                    #endif
                    elements.clear();
                    vTaskDelay(pdMS_TO_TICKS((basetime * 3) - (esp_timer_get_time() / 1000 - lastChar)));
                } else if (readbuf[i] == space) {
                    lastChar = esp_timer_get_time() / 1000;
                    printf(decodeChar(elements).c_str());
                    printf(" ");
                    #ifdef KEYBOARD_KEYER
                    sendKey(decodeChar(elements));
                    sendKey(" ");
                    #endif
                    elements.clear();
                    vTaskDelay(pdMS_TO_TICKS((basetime * 7) - (esp_timer_get_time() / 1000 - lastChar)));

                }
                fflush(stdout);
                fsync(fileno(stdout));
            }
            delete[] readbuf;
            fflush(stdout);
            fsync(fileno(stdout));
            vTaskDelay(pdMS_TO_TICKS(50));
            hasSpace = false;
            lastChar = esp_timer_get_time() / 1000;

        }
        if (gpio_get_level(recordPin)) {
            recordLock = false;
        }
        if (!gpio_get_level(recordPin) && !recordMode && !recordLock) {
            recordLock = true;
            recordMode = true;
            printf("\n start-- ");
            gpio_set_level(recLedPin, 1);
            vTaskDelay(pdMS_TO_TICKS(200));

        } else if (!gpio_get_level(recordPin) && recordMode && !recordLock) {
            recordLock = true;
            recordMode = false;
            printf(" --stop \n");
            gpio_set_level(recLedPin, 0);
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

        if (!gpio_get_level(rotA) && gpio_get_level(rotB) && !rotloc) {
            rotloc = true;
            speed += 1;
            basetime = 1200 / speed;
            printf("speed %d\n", speed);
            vTaskDelay(pdMS_TO_TICKS(50));

        }
        if (gpio_get_level(rotA) && !gpio_get_level(rotB) && !rotloc) {
            rotloc = true;
            speed -= 1;
            basetime = 1200 / speed;
            printf("speed %d\n", speed);
            vTaskDelay(pdMS_TO_TICKS(50));

        }
        if (rotloc && gpio_get_level(rotA) && gpio_get_level(rotB)) {
            rotloc = false;
        }
        fflush(stdout);
        fsync(fileno(stdout));

    }
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