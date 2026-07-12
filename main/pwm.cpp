
#include <stdio.h>
#include "driver/ledc.h"
#include "esp_err.h"
#include "sdkconfig.h"
//#include "esp_pm.h"
#include <math.h>

#if CONFIG_PM_ENABLE
#define LEDC_CLK_SRC            LEDC_USE_RC_FAST_CLK // choose a clock source that can maintain during light sleep
#else
#define LEDC_CLK_SRC            LEDC_AUTO_CLK
#endif



void initPWM(uint32_t x, int pin) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = x,
        .clk_cfg = LEDC_CLK_SRC,
        .deconfigure = false
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .gpio_num = pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags = {0},
        .deconfigure = false
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));

}

void setPWMEnable(bool x) {
    if (x) {
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, pow(2, 13) / 2));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
    } else {
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
    }
}

void setFreq(uint32_t x) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = x,
        .clk_cfg = LEDC_CLK_SRC,
        .deconfigure = false
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
}