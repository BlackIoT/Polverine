#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"

#define RED_PIN 47
#define GREEN_PIN 48
#define BLUE_PIN 38

#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_FREQUENCY 5000 // 5 kHz PWM frequency

void configure_led(int pin, ledc_channel_t channel, ledc_timer_t timer)
{
    // Configure timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = timer,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // Configure channel
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = channel,
        .timer_sel      = timer,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = pin,
        .duty           = 0,
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
}

void set_color(int r, int g, int b)
{
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, r);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, g);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_2, b);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_2);
}

void app_main(void)
{
    configure_led(RED_PIN, LEDC_CHANNEL_0, LEDC_TIMER_0);
    configure_led(GREEN_PIN, LEDC_CHANNEL_1, LEDC_TIMER_1);
    configure_led(BLUE_PIN, LEDC_CHANNEL_2, LEDC_TIMER_2);

    int step = 200;
    int max_duty = (1 << LEDC_DUTY_RES) - 1;

    while (1) {
        // Fade red up, others down
        for (int i = 0; i <= max_duty; i += step) {
            set_color(i, max_duty - i, 0);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }

        // Fade green up, red down
        for (int i = 0; i <= max_duty; i += step) {
            set_color(max_duty - i, i, 0);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }

        // Fade blue up, green down
        for (int i = 0; i <= max_duty; i += step) {
            set_color(0, max_duty - i, i);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }

        // Fade red up, blue down
        for (int i = 0; i <= max_duty; i += step) {
            set_color(i, 0, max_duty - i);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
}