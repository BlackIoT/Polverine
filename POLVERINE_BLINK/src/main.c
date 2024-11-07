#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define R_LED_PIN 47 // GPIO pin connected to onboard LED
#define G_LED_PIN 48 // GPIO pin connected to onboard LED
#define B_LED_PIN 38 // GPIO pin connected to onboard LED


void app_main(void)
{
    // Configure LED pin as output
    esp_rom_gpio_pad_select_gpio(R_LED_PIN);
    gpio_set_direction(R_LED_PIN, GPIO_MODE_OUTPUT);

    esp_rom_gpio_pad_select_gpio(G_LED_PIN);
    gpio_set_direction(G_LED_PIN, GPIO_MODE_OUTPUT);

    esp_rom_gpio_pad_select_gpio(B_LED_PIN);
    gpio_set_direction(B_LED_PIN, GPIO_MODE_OUTPUT);

    while (1) {
        // Turn LED on
        gpio_set_level(R_LED_PIN, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay 1 second
        
        // Turn LED off  
        gpio_set_level(R_LED_PIN, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay 1 second
 
       // Turn LED on
        gpio_set_level(G_LED_PIN, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay 1 second
        
        // Turn LED off  
        gpio_set_level(G_LED_PIN, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay 1 second
 
        // Turn LED on
        gpio_set_level(B_LED_PIN, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay 1 second
        
        // Turn LED off  
        gpio_set_level(B_LED_PIN, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay 1 second
     }
}