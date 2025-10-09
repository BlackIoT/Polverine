/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Common functions for protocol examples, to establish Wi-Fi or Ethernet connection.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include <string.h>
#include "protocol_common.h"
#include "common_private.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "esp_netif.h"
#include "lwip/ip4_addr.h"
#include "esp_mac.h"
#include "esp_wifi_types.h"
#include "freertos/timers.h"
#include "config.h"

#define CONFIG_POLVERINE_WIFI_CONN_MAX_RETRY 3
#define DHCP_RETRY_INTERVAL_MS 60000 // 1 minute

// Secrets moved to external configuration
#define POLVERINE_WIFI_SCAN_METHOD WIFI_ALL_CHANNEL_SCAN
#define POLVERINE_WIFI_CONNECT_AP_SORT_METHOD WIFI_CONNECT_AP_BY_SIGNAL
#define POLVERINE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#define POLVERINE_NETIF_DESC_STA "polverine_sta"


static const char *TAG = "connect";
static esp_netif_t *s_example_sta_netif = NULL;
static SemaphoreHandle_t s_semph_get_ip_addrs = NULL;
static TimerHandle_t s_dhcp_retry_timer = NULL;
static bool s_using_static_ip = false;

// Function prototypes
esp_err_t set_static_ip(void);
void dhcp_retry_timer_callback(TimerHandle_t xTimer);
esp_err_t retry_dhcp(void);

cJSON *wifi = 0;

void wifi_store_read(void)
{
    polverine_wifi_config_t config;
    
    if (config_load_wifi(&config)) {
        // Create JSON from loaded config
        if(wifi) {
            cJSON_Delete(wifi);
            wifi = 0;
        }
        
        wifi = cJSON_CreateObject();
        cJSON_AddStringToObject(wifi, "ssid", config.ssid);
        cJSON_AddStringToObject(wifi, "pwd", config.password);
        
        ESP_LOGI(TAG, "WiFi configuration read: SSID=%s", config.ssid);
    } else {
        ESP_LOGE(TAG, "Failed to load WiFi configuration");
        // Create a default JSON object to prevent crashes
        if(wifi) {
            cJSON_Delete(wifi);
            wifi = 0;
        }
        wifi = cJSON_CreateObject();
        cJSON_AddStringToObject(wifi, "ssid", "");
        cJSON_AddStringToObject(wifi, "pwd", "");
    }
}

void wifi_store_write(const char *buffer)
{
    cJSON *json = cJSON_Parse(buffer);
    
    if(json) {
        polverine_wifi_config_t config = {0};
        
        cJSON *ssid = cJSON_GetObjectItemCaseSensitive(json, "ssid");
        cJSON *pwd = cJSON_GetObjectItemCaseSensitive(json, "pwd");
        
        if (ssid && cJSON_IsString(ssid)) {
            strncpy(config.ssid, ssid->valuestring, sizeof(config.ssid) - 1);
        }
        if (pwd && cJSON_IsString(pwd)) {
            strncpy(config.password, pwd->valuestring, sizeof(config.password) - 1);
        }
        
        if (config_save_wifi(&config)) {
            ESP_LOGI(TAG, "WiFi configuration saved");
        } else {
            ESP_LOGE(TAG, "Failed to save WiFi configuration");
        }
        
        cJSON_Delete(json);
    }
}

static int s_retry_num = 0;

static void example_handler_on_wifi_disconnect(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "WiFi disconnect event received");
    s_retry_num++;
    ESP_LOGI(TAG, "Retry attempt: %d/%d", s_retry_num, CONFIG_POLVERINE_WIFI_CONN_MAX_RETRY);

    if (s_retry_num > CONFIG_POLVERINE_WIFI_CONN_MAX_RETRY) {
        ESP_LOGE(TAG, "WiFi Connect failed %d times, stop reconnect.", s_retry_num);
        /* let wifi_sta_do_connect() return */
        if (s_semph_get_ip_addrs) {
            ESP_LOGI(TAG, "Giving semaphore to unblock wifi_sta_do_connect");
            xSemaphoreGive(s_semph_get_ip_addrs);
        } else {
            ESP_LOGW(TAG, "Semaphore not available to give");
        }
        return;
    }

    ESP_LOGI(TAG, "Wi-Fi disconnected, trying to reconnect...");
    esp_err_t err = esp_wifi_connect();
    if (err == ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGE(TAG, "WiFi not started, cannot reconnect");
        return;
    }
    ESP_LOGI(TAG, "Reconnection attempt initiated");
    ESP_ERROR_CHECK(err);
}

static void polverine_handler_on_wifi_connect(void *esp_netif, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "WiFi connected event received");

    // Get more details about the connection
    wifi_ap_record_t ap_info;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Connected to SSID: %s", ap_info.ssid);
        ESP_LOGI(TAG, "Channel: %d, RSSI: %d", ap_info.primary, ap_info.rssi);
        ESP_LOGI(TAG, "BSSID: "MACSTR, MAC2STR(ap_info.bssid));

        // Log authentication mode
        const char *auth_mode = "UNKNOWN";
        switch (ap_info.authmode) {
            case WIFI_AUTH_OPEN: auth_mode = "OPEN"; break;
            case WIFI_AUTH_WEP: auth_mode = "WEP"; break;
            case WIFI_AUTH_WPA_PSK: auth_mode = "WPA_PSK"; break;
            case WIFI_AUTH_WPA2_PSK: auth_mode = "WPA2_PSK"; break;
            case WIFI_AUTH_WPA_WPA2_PSK: auth_mode = "WPA_WPA2_PSK"; break;
            case WIFI_AUTH_WPA2_ENTERPRISE: auth_mode = "WPA2_ENTERPRISE"; break;
            case WIFI_AUTH_WPA3_PSK: auth_mode = "WPA3_PSK"; break;
            case WIFI_AUTH_WPA2_WPA3_PSK: auth_mode = "WPA2_WPA3_PSK"; break;
            default: break;
        }
        ESP_LOGI(TAG, "Authentication mode: %s", auth_mode);
    } else {
        ESP_LOGW(TAG, "Failed to get AP info: %s", esp_err_to_name(err));
    }

    // Log DHCP client status
    ESP_LOGI(TAG, "Checking DHCP client status...");
    esp_netif_dhcp_status_t dhcp_status;
    err = esp_netif_dhcpc_get_status(s_example_sta_netif, &dhcp_status);
    if (err == ESP_OK) {
        const char *status_str = "UNKNOWN";
        switch (dhcp_status) {
            case ESP_NETIF_DHCP_INIT: status_str = "INIT"; break;
            case ESP_NETIF_DHCP_STARTED: status_str = "STARTED"; break;
            case ESP_NETIF_DHCP_STOPPED: status_str = "STOPPED"; break;
            default: break;
        }
        ESP_LOGI(TAG, "DHCP client status: %s", status_str);
    } else {
        ESP_LOGW(TAG, "Failed to get DHCP client status: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Waiting for IP address assignment...");
}

static void polverine_handler_on_sta_got_ip(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "Got IP event received");
    s_retry_num = 0;
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

    if (!is_our_netif(POLVERINE_NETIF_DESC_STA, event->esp_netif)) {
        ESP_LOGW(TAG, "Got IP but not for our interface");
        return;
    }

    ESP_LOGI(TAG, "Got IPv4 event: Interface \"%s\" address: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
    ESP_LOGI(TAG, "Gateway: " IPSTR ", Netmask: " IPSTR, IP2STR(&event->ip_info.gw), IP2STR(&event->ip_info.netmask));
    ESP_LOGI(TAG, "DNS Server: " IPSTR, IP2STR(&event->ip_info.gw)); // Often the gateway is also the DNS server

    if (s_semph_get_ip_addrs) {
        ESP_LOGI(TAG, "Giving semaphore to unblock wifi_sta_do_connect");
        xSemaphoreGive(s_semph_get_ip_addrs);
    } else {
        ESP_LOGW(TAG, "Semaphore not available to give");
        ESP_LOGI(TAG, "- IPv4 address: " IPSTR ",", IP2STR(&event->ip_info.ip));
    }
}


void wifi_start(void)
{
    ESP_LOGI(TAG, "Starting wifi_start...");

    ESP_LOGI(TAG, "Initializing WiFi with default config...");
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_LOGI(TAG, "WiFi initialized");

    ESP_LOGI(TAG, "Creating network interface...");
    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    // Warning: the interface desc is used in tests to capture actual connection details (IP, gw, mask)
    esp_netif_config.if_desc = POLVERINE_NETIF_DESC_STA;
    esp_netif_config.route_prio = 128;
    s_example_sta_netif = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
    ESP_LOGI(TAG, "Network interface created");

    ESP_LOGI(TAG, "Setting default WiFi handlers...");
    esp_wifi_set_default_wifi_sta_handlers();
    ESP_LOGI(TAG, "Default WiFi handlers set");

    ESP_LOGI(TAG, "Configuring WiFi storage...");
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_LOGI(TAG, "Setting WiFi mode to STA...");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_LOGI(TAG, "Starting WiFi...");
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Enable WiFi power save mode for better light sleep compatibility
    ESP_LOGI(TAG, "Enabling WiFi power save mode...");
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
    ESP_LOGI(TAG, "WiFi power save mode enabled");
    
    ESP_LOGI(TAG, "WiFi started successfully");
}


void wifi_stop(void)
{
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        return;
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(s_example_sta_netif));
    esp_netif_destroy(s_example_sta_netif);
    s_example_sta_netif = NULL;
}


esp_err_t wifi_sta_do_connect(wifi_config_t wifi_config, bool wait)
{
    ESP_LOGI(TAG, "Starting wifi_sta_do_connect...");

    if (wait) {
        ESP_LOGI(TAG, "Creating semaphore for IP address wait...");
        s_semph_get_ip_addrs = xSemaphoreCreateBinary();
        if (s_semph_get_ip_addrs == NULL) {
            ESP_LOGE(TAG, "Failed to create semaphore!");
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "Semaphore created successfully");
    }

    s_retry_num = 0;
    ESP_LOGI(TAG, "Registering event handlers...");
    ESP_LOGI(TAG, "Registering disconnect handler...");
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &example_handler_on_wifi_disconnect, NULL));
    ESP_LOGI(TAG, "Registering IP handler...");
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &polverine_handler_on_sta_got_ip, NULL));
    ESP_LOGI(TAG, "Registering connect handler...");
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &polverine_handler_on_wifi_connect, s_example_sta_netif));
    ESP_LOGI(TAG, "All event handlers registered");

    ESP_LOGI(TAG, "Connecting to SSID: %s...", wifi_config.sta.ssid);
    ESP_LOGI(TAG, "Setting WiFi configuration...");
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_LOGI(TAG, "Initiating connection...");
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connect failed! ret:0x%x", ret);
        return ret;
    }
    ESP_LOGI(TAG, "Connection initiated successfully");

    if (wait) {
        ESP_LOGI(TAG, "Waiting for IP address with timeout...");
        // Wait for 60 seconds max instead of indefinitely
        if (xSemaphoreTake(s_semph_get_ip_addrs, pdMS_TO_TICKS(60000)) == pdTRUE) {
            ESP_LOGI(TAG, "Received IP address from DHCP");
        } else {
            ESP_LOGW(TAG, "Timeout waiting for IP address from DHCP after 60 seconds!");
            ESP_LOGI(TAG, "Consider checking your router's DHCP server or increasing timeout further");

            // Try to set a static IP address as fallback
            ESP_LOGI(TAG, "Attempting to set static IP as fallback (192.168.1.250)...");
            esp_err_t err = set_static_ip();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to set static IP: %s", esp_err_to_name(err));
                // If we failed to set static IP, try to proceed anyway
                ESP_LOGE(TAG, "Device may not have network connectivity!");
                return ESP_OK;
            }

            ESP_LOGI(TAG, "Successfully set static IP address - device should be accessible at 192.168.1.250");
            return ESP_OK;
        }

        if (s_retry_num > CONFIG_POLVERINE_WIFI_CONN_MAX_RETRY) {
            ESP_LOGE(TAG, "Exceeded maximum retry attempts (%d)", CONFIG_POLVERINE_WIFI_CONN_MAX_RETRY);
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "wifi_sta_do_connect completed successfully");
    return ESP_OK;
}

esp_err_t wifi_sta_do_disconnect(void)
{
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &example_handler_on_wifi_disconnect));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &polverine_handler_on_sta_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &polverine_handler_on_wifi_connect));
    if (s_semph_get_ip_addrs) {
        vSemaphoreDelete(s_semph_get_ip_addrs);
    }
    return esp_wifi_disconnect();
}

void wifi_shutdown(void)
{
    wifi_sta_do_disconnect();
    wifi_stop();
}

// Function to retry DHCP after using static IP
esp_err_t retry_dhcp(void)
{
    if (!s_using_static_ip) {
        ESP_LOGI(TAG, "Not using static IP, no need to retry DHCP");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Retrying DHCP after using static IP...");

    // Start DHCP client
    ESP_LOGI(TAG, "Starting DHCP client...");
    esp_err_t err = esp_netif_dhcpc_start(s_example_sta_netif);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start DHCP client: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "DHCP client started");

    // Wait a bit for DHCP to get an address
    ESP_LOGI(TAG, "Waiting for DHCP to get an address...");
    vTaskDelay(pdMS_TO_TICKS(5000));

    // Check if we got an IP address
    esp_netif_ip_info_t ip_info;
    err = esp_netif_get_ip_info(s_example_sta_netif, &ip_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get IP info: %s", esp_err_to_name(err));
        return err;
    }

    // Check if the IP is still the static one or if DHCP provided a new one
    if (ip_info.ip.addr == 0 || ip4_addr_isany_val(ip_info.ip)) {
        ESP_LOGW(TAG, "DHCP failed to get an IP address, keeping static IP");
        // Stop DHCP client and keep static IP
        esp_netif_dhcpc_stop(s_example_sta_netif);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "DHCP successfully got IP address: " IPSTR, IP2STR(&ip_info.ip));
    s_using_static_ip = false;

    // If we have a timer, delete it as we don't need it anymore
    if (s_dhcp_retry_timer != NULL) {
        xTimerDelete(s_dhcp_retry_timer, 0);
        s_dhcp_retry_timer = NULL;
    }

    return ESP_OK;
}

// Timer callback to retry DHCP
void dhcp_retry_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "DHCP retry timer triggered");
    retry_dhcp();
}

// Function to set a static IP address as a fallback
esp_err_t set_static_ip(void)
{
    ESP_LOGI(TAG, "Setting static IP address as fallback...");

    // Stop DHCP client
    ESP_LOGI(TAG, "Stopping DHCP client...");
    esp_err_t err = esp_netif_dhcpc_stop(s_example_sta_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        ESP_LOGE(TAG, "Failed to stop DHCP client: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "DHCP client stopped");

    // Set static IP address (192.168.1.250)
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 1, 250);
    IP4_ADDR(&ip_info.gw, 192, 168, 1, 1);  // Assuming gateway is 192.168.1.1
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);

    ESP_LOGI(TAG, "Setting static IP: " IPSTR ", Gateway: " IPSTR ", Netmask: " IPSTR,
             IP2STR(&ip_info.ip), IP2STR(&ip_info.gw), IP2STR(&ip_info.netmask));

    err = esp_netif_set_ip_info(s_example_sta_netif, &ip_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set static IP: %s", esp_err_to_name(err));
        return err;
    }

    // Set DNS server (using gateway as DNS server)
    esp_netif_dns_info_t dns_info;
    dns_info.ip.u_addr.ip4.addr = ip_info.gw.addr;
    dns_info.ip.type = ESP_IPADDR_TYPE_V4;

    ESP_LOGI(TAG, "Setting DNS server: " IPSTR, IP2STR(&ip_info.gw));
    err = esp_netif_set_dns_info(s_example_sta_netif, ESP_NETIF_DNS_MAIN, &dns_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set DNS server: %s", esp_err_to_name(err));
        // Continue anyway, not critical
    }

    // Mark that we're using static IP
    s_using_static_ip = true;

    // Create a timer to periodically retry DHCP
    if (s_dhcp_retry_timer == NULL) {
        s_dhcp_retry_timer = xTimerCreate(
            "dhcp_retry_timer",
            pdMS_TO_TICKS(DHCP_RETRY_INTERVAL_MS),
            pdTRUE,  // Auto reload
            NULL,
            dhcp_retry_timer_callback
        );

        if (s_dhcp_retry_timer == NULL) {
            ESP_LOGE(TAG, "Failed to create DHCP retry timer");
        } else {
            ESP_LOGI(TAG, "Starting DHCP retry timer with interval %d ms", DHCP_RETRY_INTERVAL_MS);
            xTimerStart(s_dhcp_retry_timer, 0);
        }
    }

    ESP_LOGI(TAG, "Static IP configuration complete");
    return ESP_OK;
}

esp_err_t wifi_connect(void)
{
    ESP_LOGI(TAG, "Starting wifi_connect...");

    ESP_LOGI(TAG, "Reading WiFi configuration from storage...");
//    wifi_store_write(DEFAULT_POLVERINE_WIFI);
    wifi_store_read();
    
    // Check if wifi JSON object was created successfully
    if (!wifi) {
        ESP_LOGE(TAG, "Failed to create WiFi configuration JSON object");
        return ESP_FAIL;
    }
    
    cJSON *ssid_item = cJSON_GetObjectItemCaseSensitive(wifi, "ssid");
    cJSON *pwd_item = cJSON_GetObjectItemCaseSensitive(wifi, "pwd");
    
    if (!ssid_item || !pwd_item) {
        ESP_LOGE(TAG, "WiFi configuration JSON missing required fields");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "WiFi configuration read: SSID=%s", ssid_item->valuestring);

    ESP_LOGI(TAG, "Starting WiFi...");
    wifi_start();
    ESP_LOGI(TAG, "WiFi started, preparing connection configuration...");

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
            .scan_method = POLVERINE_WIFI_SCAN_METHOD,
            .sort_method = POLVERINE_WIFI_CONNECT_AP_SORT_METHOD,
            .threshold.rssi = 0,
            .threshold.authmode = POLVERINE_WIFI_SCAN_AUTH_MODE_THRESHOLD,
        },
    };

    ESP_LOGI(TAG, "Copying SSID and password from configuration...");
    strncpy((char *)&wifi_config.sta.ssid, ssid_item->valuestring, sizeof(wifi_config.sta.ssid));
    strncpy((char *)&wifi_config.sta.password, pwd_item->valuestring, sizeof(wifi_config.sta.password));
    ESP_LOGI(TAG, "Configuration prepared, connecting to SSID: %s", wifi_config.sta.ssid);

    ESP_LOGI(TAG, "Calling wifi_sta_do_connect...");
    esp_err_t ret = wifi_sta_do_connect(wifi_config, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "wifi_sta_do_connect failed with error: 0x%x", ret);
    } else {
        ESP_LOGI(TAG, "wifi_sta_do_connect successful");
    }
    return ret;
}
