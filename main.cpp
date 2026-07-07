#include <iostream>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "pico/time.h"
#include "hardware/uart.h"
#include "hardware/pwm.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "chars.h"
#include "sstream"

#include "bsp/board_api.h"
#include "tusb.h"

#include "usb_descriptors.h"

#define FLASH_TARGET_OFFSET (2 * 1024 * 1024 - FLASH_SECTOR_SIZE)

const uint8_t* contents;
uint32_t saved_interrupts;

int dit_state;
int dah_state;

int last = -1;
int curent = -1;
int next = -1;

int speed = 22;
int tone = 440;

int level = ((125000000 / 125) / tone) / 2;
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
        pwm_set_gpio_level(pwm_pin, level);
        #ifdef MIDI_KEYER
        msg[0] = 0x09; // Note On - Channel 1
        msg[1] = 0x90;   // Note Number
        msg[2] = 1;
        msg[3] = 0x7F;  // Velocity
        tud_midi_n_stream_write(0, 0, msg, 4);
        #endif
    } else {
        pwm_set_gpio_level(pwm_pin, 0);
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

int64_t coolDown(alarm_id_t id, void* user_data) {
    curent = -1;
    return 0;
}

int64_t turnOff(alarm_id_t id, void* user_data) {
    key(false);
    add_alarm_in_ms(basetime, *coolDown, user_data, false);
    lastChar = to_ms_since_boot(get_absolute_time());
    elements.push_back(*((int*)user_data));
    if (recordMode) {
        recordArr.push_back(*((int*)user_data));
    }

    return 0;
}

void doDit() {
    key(true);
    add_alarm_in_ms(basetime, *turnOff, (void*)&dit, false);
    hasSpace = false;
    curent = dit;
    last = dit;
}

void doDah() {
    key(true);
    add_alarm_in_ms(basetime * 3, *turnOff, (void*)&dah, false);
    hasSpace = false;
    curent = dah;
    last = dah;
}

int main() {
    stdio_init_all();
    uart_init(uart0, 115200);



    gpio_set_function(17, GPIO_FUNC_UART);
    gpio_set_function(16, GPIO_FUNC_UART);

    gpio_set_function(pwm_pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pwm_pin);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv_int_frac(&config, 125, 0);
    pwm_config_set_wrap(&config, (125000000 / 125) / tone);
    pwm_init(slice_num, &config, true);
    key(true);
    sleep_ms(100);
    key(false);

    board_init();
    tusb_init();
    #ifdef KEYBOARD_KEYER

    // init device stack on configured roothub port
    const tusb_rhport_init_t rh_init = {
      .role = TUSB_ROLE_DEVICE,
      .speed = TUD_OPT_HIGH_SPEED ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL
    };
    TU_ASSERT(tud_rhport_init(BOARD_TUD_RHPORT, &rh_init));
    #endif
    gpio_init(dit);
    gpio_set_dir(dit, GPIO_IN);
    gpio_set_pulls(dit, true, false);

    gpio_init(dah);
    gpio_set_dir(dah, GPIO_IN);
    gpio_set_pulls(dah, true, false);


    gpio_init(recordPin);
    gpio_set_dir(recordPin, GPIO_IN);
    gpio_set_pulls(recordPin, true, false);

    gpio_init(playPin);
    gpio_set_dir(playPin, GPIO_IN);
    gpio_set_pulls(playPin, true, false);

    while (true) {
        tud_task();

        dit_state = !gpio_get(dit);
        dah_state = !gpio_get(dah);
        if (dah_state && dit_state) {
            //uart_puts(uart0, "both\n");
            next = 3;
        } else if (dit_state && !dah_state) {
            //uart_puts(uart0, "dit\n");
            if (curent == dah || curent == -1) {
                next = dit;
            }
        } else if (dah_state && !dit_state) {
            //uart_puts(uart0, "dah\n");

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
        if (to_ms_since_boot(get_absolute_time()) - lastChar > basetime * 7 && !hasSpace && curent == -1) {
            uart_puts(uart0, " ");
            #ifdef KEYBOARD_KEYER
            sendKey(" ");
            #endif

            hasSpace = true;
            if (recordMode) {
                recordArr.pop_back();
                recordArr.push_back(space);
            }
        } else if (elements.size() > 0 && to_ms_since_boot(get_absolute_time()) - lastChar > basetime * 2.8 && curent == -1) {
            uart_puts(uart0, decodeChar(elements).c_str());
            #ifdef KEYBOARD_KEYER
            sendKey(decodeChar(elements));
            #endif
            elements.clear();
            if (recordMode && recordArr.size() > 0) {
                recordArr.push_back(gap);
            }
        }

        if (!gpio_get(playPin)) {
            contents = (const uint8_t*)(XIP_BASE + FLASH_TARGET_OFFSET);


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
                    uart_puts(uart0, decodeChar(elements).c_str());
                    elements.clear();
                    sleep_ms(basetime * 3);
                } else if (contents[i] == space) {
                    uart_puts(uart0, decodeChar(elements).c_str());
                    elements.clear();
                    uart_puts(uart0, " ");
                    sleep_ms(basetime * 7);
                }
            }
        }

        if (gpio_get(recordPin)) {
            recordLock = false;
        }
        if (!gpio_get(recordPin) && !recordMode && !recordLock) {
            recordLock = true;
            recordMode = true;
            uart_puts(uart0, " start-- ");
        } else if (!gpio_get(recordPin) && recordMode && !recordLock) {
            recordLock = true;
            recordMode = false;
            uart_puts(uart0, " --stop ");
            if (recordArr.size() != 0) {
                recordArr.pop_back();
                recordArr.push_back(end);

                alignas(4) uint8_t write_buf[FLASH_PAGE_SIZE];
                for (int i = 0; i < FLASH_PAGE_SIZE; i++) {
                    write_buf[i] = 4;
                }

                for (int i = 0; i < recordArr.size(); i++) {
                    write_buf[i] = recordArr.at(i);
                }

                saved_interrupts = save_and_disable_interrupts();
                flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
                flash_range_program(FLASH_TARGET_OFFSET, write_buf, FLASH_PAGE_SIZE);
                restore_interrupts(saved_interrupts);
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
