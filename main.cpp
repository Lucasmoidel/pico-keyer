#include <iostream>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "pico/time.h"
#include "hardware/uart.h"
#include "hardware/pwm.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "chars.h"
#include "sstream"

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO astreamignments
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9

#define FLASH_TARGET_OFFSET (2 * 1024 * 1024 - FLASH_SECTOR_SIZE)
const uint8_t *contents;

int dit_state;
int dah_state;

int last = -1;
int curent = -1;
int next = -1;
int speed = 22;
int tone = 430;
int level = ((125000000/125)/tone)/2;
int basetime = 1200/speed;
//std::stringstream ss;
std::string str;
std::vector<int> elements;
uint32_t lastchar = 0;
bool recordMode = false;
std::vector<int> recordArr;

std::string decodeChar(std::vector<int> elements){
    std::stringstream stream;
    bool match = true;
    for(int i = 0; i < chars.size(); i++){
        if(chars.at(i).size() == elements.size()){
            for (int x = 0; x < elements.size(); x++){
                if (elements.at(x) != chars.at(i).at(x)){
                    match = false;
                    break;
                }
            }
        }   else{
            match = false;
        }
        if (match){
            stream << strs.at(i);
            return stream.str();
        }
        match = true;
    }
    return "";
}

void key(bool x){
    if (x){
        pwm_set_gpio_level(pwm_pin, level);
    } else{
        pwm_set_gpio_level(pwm_pin, 0);
    }
}

int64_t coolDown(alarm_id_t id, void *user_data) {
    curent = -1;
    return 0;
}

int64_t turnOff(alarm_id_t id, void *user_data) {
    key(false);
    add_alarm_in_ms(basetime, *coolDown, user_data, false);
    lastchar = to_ms_since_boot(get_absolute_time());
    elements.push_back(*((int*)user_data));
    return 0;
}

void doDit(){
    key(true);
    add_alarm_in_ms(basetime, *turnOff, (void*) &dit, false);
    curent = dit;
    last = dit;
}

void doDah(){
    key(true);
    add_alarm_in_ms(basetime*3, *turnOff, (void*) &dah, false);
    curent = dah;
    last = dah;

}

int main()
{
    stdio_init_all();

    alignas(4) uint8_t write_buf[FLASH_PAGE_SIZE];
    std::vector<uint8_t> thingarr;
    std::string thingstr = "CQ POTA DE KO6LBA";
    int z = 0;
    for (int i = 0; i < thingstr.length(); i++){
        if(z != 0 && (write_buf[z-1] == 0 || write_buf[z-1] == 1) && thingstr.substr(i, 1) != sp){
            write_buf[z] = gap;
            z++;
        }
        if(thingstr.substr(i, 1) == sp){
                write_buf[z] = space;
                z++;
        } else {
            for (int x = 0; x < strs.size(); x++){
                if (thingstr.substr(i, 1) == strs.at(x)){
                    for (int y = 0; y < chars.at(x).size(); y++){
                        write_buf[z] = chars.at(x).at(y);
                        z++;
                    }
                } 
            } 
        }

    }
    write_buf[z] = end;

    uint32_t saved_interrupts = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    restore_interrupts(saved_interrupts);
    saved_interrupts = save_and_disable_interrupts();
    flash_range_program(FLASH_TARGET_OFFSET, write_buf, FLASH_PAGE_SIZE);
    restore_interrupts(saved_interrupts);

    uart_init(uart0, 115200);

    gpio_set_function(17, GPIO_FUNC_UART);
    gpio_set_function(16, GPIO_FUNC_UART);

    gpio_set_function(pwm_pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pwm_pin);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv_int_frac(&config, 125, 0); 
    pwm_config_set_wrap(&config, (125000000/125)/tone); 
    pwm_init(slice_num, &config, true);
    key(true);
    sleep_ms(100);
    key(false);

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

    while (true){
        // Read the state of the input pin
        dit_state = !gpio_get(dit);
        dah_state = !gpio_get(dah);
        if (dah_state && dit_state) {
            //uart_puts(uart0, "both\n");
            next = 3;
        } else if (dit_state && !dah_state) {
            //uart_puts(uart0, "dit\n");
            if (curent == dah || curent == -1){
                next = dit;
            }
        } else if (dah_state && !dit_state) {
            //uart_puts(uart0, "dah\n");

            if (curent == dit || curent == -1){
                next = dah;
            }
        }

        if(curent == -1){
            if (next == dit){
                doDit();
                next = -1;
            } else if (next == dah){
                doDah();
                next = -1;
            } else if (next == 3){
                if(last == dit){
                    doDah();
                } else if (last == dah)
                {
                    doDit();
                }
                next = -1;
            }
        }
        if(elements.size()>0 && to_ms_since_boot(get_absolute_time())-lastchar > basetime*2.8 && curent == -1){
            uart_puts(uart0, decodeChar(elements).c_str());

            elements.clear();
        }
        if(!gpio_get(playPin)){
            contents = (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET);

            
            for(int i = 0; i < FLASH_PAGE_SIZE;i++){
                if (contents[i] == end){
                    uart_puts(uart0, " ");
                    sleep_ms(basetime*7);
                    break;
                } else if(contents[i] == dit){
                    key(true);
                    sleep_ms(basetime);
                    key(false);
                    sleep_ms(basetime);
                    elements.push_back(dit);
                } else if(contents[i] == dah){
                    key(true);
                    sleep_ms(basetime*3);
                    key(false);
                    sleep_ms(basetime);
                    elements.push_back(dah);
                } else if(contents[i] == gap){
                    uart_puts(uart0, decodeChar(elements).c_str());
                    elements.clear();
                    sleep_ms(basetime*3);
                } else if(contents[i] == space){
                    uart_puts(uart0, " ");
                    sleep_ms(basetime*7);
                }
            }

        }
    }
}
