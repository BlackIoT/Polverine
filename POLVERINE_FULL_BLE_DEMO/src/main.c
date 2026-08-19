/* BLE Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_mac.h"
#include "peripherals.h"
#include "esp_pm.h"

#include "ble_main.h"


extern void bmv080_app_start();
extern void bme690_app_start();
extern void usb_app_start(void);
const char *TAG = "main";
char uniqueId[13] = {0};
char shortId[7] = {0};

void pm_init(void)
{
static const char *TAG = "power_management";
    #if CONFIG_PM_ENABLE
        esp_pm_config_t pm_config = {
            .max_freq_mhz = 80,
            .min_freq_mhz = 80,
            .light_sleep_enable = true
        };

        esp_err_t err = esp_pm_configure(&pm_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure power management: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Power management configured successfully");
        }
    #else
        ESP_LOGW(TAG, "Power management is not enabled in sdkconfig");
    #endif
}

/* RGB LED pads: plain outputs, driven with gpio_set_level() from the sensor tasks.
 * Do NOT gpio_hold_en() them here: on the ESP32-S3 the pad hold latches the output
 * level until gpio_hold_dis(), so every later gpio_set_level() would be ignored and
 * no LED indication would ever appear. gpio_sleep_sel_dis() is what keeps the pad
 * state through automatic light sleep instead (see esp_pm PM_SLP_DISABLE_GPIO). */
void gpio_init(void)
{
    esp_rom_gpio_pad_select_gpio(R_LED_PIN);
    gpio_set_direction(R_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(R_LED_PIN, 0);
    gpio_sleep_sel_dis(R_LED_PIN);

    esp_rom_gpio_pad_select_gpio(G_LED_PIN);
    gpio_set_direction(G_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(G_LED_PIN, 0);
    gpio_sleep_sel_dis(G_LED_PIN);

    esp_rom_gpio_pad_select_gpio(B_LED_PIN);
    gpio_set_direction(B_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(B_LED_PIN, 0);
    gpio_sleep_sel_dis(B_LED_PIN);
}


void app_main(void)
{
    
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    snprintf(uniqueId, sizeof(uniqueId), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    snprintf(shortId, sizeof(shortId), "%02X%02X%02X", mac[3], mac[4], mac[5]);

    gpio_init();
    pm_init();

    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
 
    esp_log_level_set("*", ESP_LOG_ERROR);
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    bmv080_app_start();

    bme690_app_start();

    usb_app_start();

    ble_init();
}