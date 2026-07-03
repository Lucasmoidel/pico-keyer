#include <iostream>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "pico/time.h"
#include "hardware/uart.h"
#include "hardware/pwm.h"
#include "chars.h"
#include "sstream"

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9

int pwm_pin = 18;

int last = -1;
int curent = -1;
int next = -1;
int speed = 22;
int tone = 430;
int level = ((125000000/125)/tone)/2;
std::stringstream ss;
std::string str;
std::vector<int> elements;
uint32_t lastchar = 0;
int64_t coolDown(alarm_id_t id, void *user_data) {
    curent = -1;
    lastchar = to_ms_since_boot(get_absolute_time());
    elements.push_back(*((int*)user_data));
    return 0;
}

int64_t turnOff(alarm_id_t id, void *user_data) {
    pwm_set_gpio_level(pwm_pin, 0);
    add_alarm_in_ms(1200/speed, *coolDown, user_data, false);

    return 0;
}

void dodit(){
    pwm_set_gpio_level(pwm_pin, level);
    add_alarm_in_ms(1200/speed, *turnOff, (void*) &dit, false);
    curent = dit;
    last = dit;
}

void dodah(){
    pwm_set_gpio_level(pwm_pin, level);
    add_alarm_in_ms(3600/speed, *turnOff, (void*) &dah, false);
    curent = dah;
    last = dah;

}

int main()
{
    stdio_init_all();

    uart_init(uart0, 115200);

    gpio_set_function(17, GPIO_FUNC_UART);
    gpio_set_function(16, GPIO_FUNC_UART);

    gpio_set_function(pwm_pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pwm_pin);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv_int_frac(&config, 125, 0); 
    pwm_config_set_wrap(&config, (125000000/125)/tone); 
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(pwm_pin, level);

    gpio_init(dit);
    gpio_init(dah);
    gpio_set_dir(dit, GPIO_IN);
    gpio_set_dir(dah, GPIO_IN);

    gpio_set_pulls(dit, true, false);
    gpio_set_pulls(dah, true, false);
    bool dit_state = !gpio_get(dit);
    bool dah_state = !gpio_get(dah);
    
    bool match = true;
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
                dodit();
                next = -1;
            } else if (next == dah){
                dodah();
                next = -1;
            } else if (next == 3){
                if(last == dit){
                    dodah();
                } else if (last == dah)
                {
                    dodit();
                }
                next = -1;
            }
        }
        if(elements.size()>0 && to_ms_since_boot(get_absolute_time())-lastchar > 5000/speed){
            // ss << "\n";
            // for (int i = 0; i < elements.size(); i++){
                
            //     if(elements.at(i) == dit){
            //         ss << "dit ";
            //     } else if(elements.at(i) == dah){
            //         ss << "dah ";
            //     }
            // }
            
            str = ss.str();
            ss.str(""); 
            ss.clear();
            uart_puts(uart0, str.c_str());
            //uart_puts(uart0, "doit\n");
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
                    ss << strs.at(i);
                    //ss<<"\n";
                    str = ss.str();
                    uart_puts(uart0, str.c_str());
                    ss.str(""); 
                    ss.clear();
                }
                match = true;
            }
            elements.clear();
        }
    }
}
