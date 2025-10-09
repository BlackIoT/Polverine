/* MQTT (over TCP) Example

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

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "esp_mac.h"
#include "mqtt_client.h"
#include "peripherals.h"
#include "esp_pm.h"
#include "config.h"
#include "wifi_provisioning.h"
#include "button_handler.h"


extern void bmv080_app_start();
extern void bme690_app_start();
extern void mqtt_app_start(void);
extern void usb_app_start(void);
extern const char *TAG;
char uniqueId[13] = {0};
char shortId[7] = {0};

extern void mqtt_default_init(const char *id);

void pm_init(void)
{
static const char *TAG = "power_management";
    #if CONFIG_PM_ENABLE
        esp_pm_config_t pm_config = {
            .max_freq_mhz = 160,
            .min_freq_mhz = 80,
            .light_sleep_enable = true  // Enable light sleep with WiFi power save
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

void gpio_init(void)
{
    esp_rom_gpio_pad_select_gpio(R_LED_PIN);
    gpio_set_direction(R_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(R_LED_PIN, 0);
    gpio_hold_en(R_LED_PIN);

    esp_rom_gpio_pad_select_gpio(G_LED_PIN);
    gpio_set_direction(G_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(G_LED_PIN, 0);
    gpio_hold_en(G_LED_PIN);

    esp_rom_gpio_pad_select_gpio(B_LED_PIN);
    gpio_set_direction(B_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(B_LED_PIN, 0);
    gpio_hold_en(B_LED_PIN);
}


void app_main(void)
{

    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    snprintf(uniqueId, sizeof(uniqueId), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    snprintf(shortId, sizeof(shortId), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    mqtt_default_init(shortId);

    // led_control_init(); // LED functionality removed
    gpio_init();
    pm_init();

    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    // Enable more verbose logging for troubleshooting
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("connect", ESP_LOG_DEBUG);
    esp_log_level_set("wifi", ESP_LOG_DEBUG);
    esp_log_level_set("mqtt_client", ESP_LOG_DEBUG);
    esp_log_level_set("mqtt_polverine", ESP_LOG_DEBUG);
    esp_log_level_set("transport_base", ESP_LOG_DEBUG);
    esp_log_level_set("esp-tls", ESP_LOG_DEBUG);
    esp_log_level_set("transport", ESP_LOG_DEBUG);
    esp_log_level_set("outbox", ESP_LOG_DEBUG);
    ESP_LOGI(TAG, "Initializing NVS flash...");
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_LOGI(TAG, "NVS flash initialized");
    
    // Initialize configuration system
    if (!config_init()) {
        ESP_LOGE(TAG, "Failed to initialize configuration system");
    }
    
    // Initialize button handler for configuration reset
    button_handler_init();

    ESP_LOGI(TAG, "Initializing network interface...");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_LOGI(TAG, "Network interface initialized");

    ESP_LOGI(TAG, "Creating default event loop...");
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_LOGI(TAG, "Default event loop created");

    // Check if provisioning is needed
    if (provisioning_is_needed()) {
        ESP_LOGI(TAG, "No WiFi credentials found. Starting provisioning...");
        
        // Initialize WiFi
        esp_netif_create_default_wifi_ap();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        
        // Start provisioning
        provisioning_start(shortId);
        
        // Keep the provisioning running
        while (provisioning_is_active()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    } else {
        // Normal operation - connect to configured WiFi
        ESP_LOGI(TAG, "Starting network connection...");
        esp_err_t ret = polverine_connect();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Network connection failed with error: 0x%x", ret);
            ESP_LOGI(TAG, "Starting provisioning mode...");
            
            // Failed to connect, start provisioning
            esp_netif_create_default_wifi_ap();
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            ESP_ERROR_CHECK(esp_wifi_init(&cfg));
            
            provisioning_start(shortId);
            
            // Keep the provisioning running
            while (provisioning_is_active()) {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        } else {
            ESP_LOGI(TAG, "Network connection established");
        }
    }

    ESP_LOGI(TAG, "Starting BMV080 application...");
    bmv080_app_start();
    ESP_LOGI(TAG, "BMV080 application started");

    ESP_LOGI(TAG, "Starting BME690 application...");
    bme690_app_start();
    ESP_LOGI(TAG, "BME690 application started");

    ESP_LOGI(TAG, "Starting MQTT application...");
    mqtt_app_start();
    ESP_LOGI(TAG, "MQTT application started");

    ESP_LOGI(TAG, "Starting USB application...");
    usb_app_start();
    ESP_LOGI(TAG, "USB application started");

    // LED test removed

    ESP_LOGI(TAG, "Startup complete!");
}
