#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "hardware/pwm.h"

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9

bool on;
bool canstart;

int64_t coolDown(alarm_id_t id, void *user_data) {

    canstart = true;
    return 0;
}

int64_t turnOff(alarm_id_t id, void *user_data) {
    gpio_put(16, 0); 
    on=false;
    add_alarm_in_ms(400, *coolDown, NULL, false);
    return 0;
}



int main()
{
    stdio_init_all();

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    uart_init(uart0, 115200);

    gpio_set_function(0, GPIO_FUNC_UART);
    gpio_set_function(1, GPIO_FUNC_UART);

    gpio_init(16);
    gpio_set_dir(16, GPIO_OUT);

    // Turn the LED on (set pin to HIGH)
    
    const uint dit = 14;
    const uint dah = 15;
    gpio_init(dit);
    gpio_init(dah);
    gpio_set_dir(dit, GPIO_IN);
    gpio_set_dir(dah, GPIO_IN);

    gpio_set_pulls(dit, false, true);
    gpio_set_pulls(dah, false, true);
    bool dit_state = gpio_get(dit);
    bool dah_state = gpio_get(dah);
    on = false;
    canstart = true;
    while (true){
        // Read the state of the input pin
        dit_state = gpio_get(dit);
        dah_state = gpio_get(dah);

        if (dit_state && !dah_state) {
            if(!on && canstart){
                on=true;
                canstart = false;
                gpio_put(16, 1);
                add_alarm_in_ms(200, *turnOff, NULL, false);
            }
        }
        if (dah_state && !dit_state) {
            gpio_put(16, 1); 
        }

        if(!dit_state && !dah_state){
            gpio_put(16, 0); 
            on=false;
            canstart=true;
        }
        //sleep_ms(10); // Small delay
    }
}
