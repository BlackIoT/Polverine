#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/ledc.h"
#include <stdio.h>
#include "peripherals.h"
#include "leds.h"

char stsMain = STATUS_BUSY;
char stsBMV080 = STATUS_BUSY;
char stsBME690 = STATUS_BUSY;

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
        .gpio_num       = pin,
        .duty           = 0,
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
}

void leds_init(void)
{
    configure_led(R_LED_PIN, LEDC_CHANNEL_0, LEDC_TIMER_0);
    configure_led(G_LED_PIN, LEDC_CHANNEL_1, LEDC_TIMER_1);
    configure_led(B_LED_PIN, LEDC_CHANNEL_2, LEDC_TIMER_2);
}

void button_init(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;  // Disable interrupt
    io_conf.mode = GPIO_MODE_INPUT;         // Set as input mode
    io_conf.pin_bit_mask = (1ULL << BOOT_BUTTON);  // Bit mask for GPIO0
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;     // Enable pull-up
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; // Disable pull-down
    gpio_config(&io_conf);
    
}

void set_r(int r)
{
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, r);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
}

void set_g(int g)
{
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, g);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);
}

void set_b(int b)
{
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_2, b);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_2);
}

void delay(uint32_t period)
{
    vTaskDelay(period / portTICK_PERIOD_MS);
}

#define MAX_DUTY ((1 << LEDC_DUTY_RES) - 1)
#define LED_DUTY (MAX_DUTY/20)
#define LED_OFF 0

void leds_task(void *pvParameter)
{
  //printf("Starting LEDs task\r\n");



  for(;;) {
    if((stsBME690 == STATUS_ERROR) && (stsBMV080 == STATUS_ERROR))
        set_r(LED_DUTY);
    else
        set_r(LED_OFF);
    if(stsBME690 == STATUS_OK)
        set_g(LED_DUTY);
    else
        set_g(LED_OFF);

    if(stsBMV080 == STATUS_OK)
        set_b(LED_DUTY);
    else
        set_b(LED_OFF);

    if((stsBME690 == STATUS_OK) && (stsBMV080 == STATUS_OK))
    {
        delay(10);
    }
    else
    {
        delay(500);
    }
    // Check if the button is pressed
    int button_state = gpio_get_level(BOOT_BUTTON);
    if (button_state == 0) // Button pressed
    {
        set_b(LED_DUTY);
    }
    else // Button released
    {
        set_b(LED_OFF);
    }
    set_r(LED_OFF);
    set_g(LED_OFF);
    if((stsBME690 == STATUS_OK) && (stsBMV080 == STATUS_OK))
    {
        delay(990);
    }
    else
    {
        delay(500);
    }
    }
    vTaskDelete(NULL);
}


void leds_app_start() 
{
  xTaskCreate(&leds_task, "leds_task", 60 * 1024, NULL, configMAX_PRIORITIES - 1, NULL);
}
