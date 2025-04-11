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

#define CONFIG_POLVERINE_WIFI_CONN_MAX_RETRY 3

const char *DEFAULT_POLVERINE_WIFI = "{\"ssid\":\"UALLSIAP\",\"pwd\":\"bibolbibol\"}";
#define POLVERINE_WIFI_SCAN_METHOD WIFI_ALL_CHANNEL_SCAN
#define POLVERINE_WIFI_CONNECT_AP_SORT_METHOD WIFI_CONNECT_AP_BY_SIGNAL
#define POLVERINE_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#define POLVERINE_NETIF_DESC_STA "polverine_sta"


static const char *TAG = "connect";
static esp_netif_t *s_example_sta_netif = NULL;
static SemaphoreHandle_t s_semph_get_ip_addrs = NULL;

cJSON *wifi = 0;

void wifi_store_read(void)
{
    
static char buffer[128] = {0};

    nvs_handle_t handle;
    esp_err_t err = nvs_open("WIFI", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        size_t length = sizeof(buffer);
        err = nvs_get_str(handle, "JSON", buffer, &length);
        if (err == ESP_OK) {
            if(wifi)
            {
                cJSON_Delete(wifi);
                wifi = 0;
            }
            wifi = cJSON_Parse(buffer);        
        }
    nvs_close(handle);
    }

    if(!wifi)
    {
        wifi = cJSON_Parse(DEFAULT_POLVERINE_WIFI);        
    }
}

void wifi_store_write(const char *buffer)
{
    cJSON *json = cJSON_Parse(buffer);

    if(json)
    {
        nvs_handle_t handle;
        esp_err_t err = nvs_open("WIFI", NVS_READWRITE, &handle);
        if (err == ESP_OK) {
            err = nvs_set_str(handle, "JSON", cJSON_Print(json));
        nvs_close(handle);
        }
    }
}

static int s_retry_num = 0;

static void example_handler_on_wifi_disconnect(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    s_retry_num++;
    if (s_retry_num > CONFIG_POLVERINE_WIFI_CONN_MAX_RETRY) {
        ESP_LOGI(TAG, "WiFi Connect failed %d times, stop reconnect.", s_retry_num);
        /* let wifi_sta_do_connect() return */
        if (s_semph_get_ip_addrs) {
            xSemaphoreGive(s_semph_get_ip_addrs);
        }
        return;
    }
    ESP_LOGI(TAG, "Wi-Fi disconnected, trying to reconnect...");
    esp_err_t err = esp_wifi_connect();
    if (err == ESP_ERR_WIFI_NOT_STARTED) {
        return;
    }
    ESP_ERROR_CHECK(err);
}

static void polverine_handler_on_wifi_connect(void *esp_netif, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
}

static void polverine_handler_on_sta_got_ip(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    s_retry_num = 0;
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    if (!is_our_netif(POLVERINE_NETIF_DESC_STA, event->esp_netif)) {
        return;
    }
    ESP_LOGI(TAG, "Got IPv4 event: Interface \"%s\" address: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
    if (s_semph_get_ip_addrs) {
        xSemaphoreGive(s_semph_get_ip_addrs);
    } else {
        ESP_LOGI(TAG, "- IPv4 address: " IPSTR ",", IP2STR(&event->ip_info.ip));
    }
}


void wifi_start(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    // Warning: the interface desc is used in tests to capture actual connection details (IP, gw, mask)
    esp_netif_config.if_desc = POLVERINE_NETIF_DESC_STA;
    esp_netif_config.route_prio = 128;
    s_example_sta_netif = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
    esp_wifi_set_default_wifi_sta_handlers();

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
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
    if (wait) {
        s_semph_get_ip_addrs = xSemaphoreCreateBinary();
        if (s_semph_get_ip_addrs == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    s_retry_num = 0;
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &example_handler_on_wifi_disconnect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &polverine_handler_on_sta_got_ip, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &polverine_handler_on_wifi_connect, s_example_sta_netif));

    ESP_LOGI(TAG, "Connecting to %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connect failed! ret:%x", ret);
        return ret;
    }
    if (wait) {
        ESP_LOGI(TAG, "Waiting for IP(s)");
        xSemaphoreTake(s_semph_get_ip_addrs, portMAX_DELAY);
        if (s_retry_num > CONFIG_POLVERINE_WIFI_CONN_MAX_RETRY) {
            return ESP_FAIL;
        }
    }
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

esp_err_t wifi_connect(void)
{
    ESP_LOGI(TAG, "Start connect.");
//    wifi_store_write(DEFAULT_POLVERINE_WIFI);
    wifi_store_read();
    wifi_start();
    wifi_config_t wifi_config = {
        .sta = {
//            .ssid = CONFIG_POLVERINE_WIFI_SSID,
//            .password = memcpy(CONFIG_POLVERINE_WIFI_PASSWORD,64),
            .scan_method = POLVERINE_WIFI_SCAN_METHOD,
            .sort_method = POLVERINE_WIFI_CONNECT_AP_SORT_METHOD,
            .threshold.rssi = 0,
            .threshold.authmode = POLVERINE_WIFI_SCAN_AUTH_MODE_THRESHOLD,
        },
    };
    strncpy((char *)&wifi_config.sta.ssid, cJSON_GetObjectItemCaseSensitive(wifi, "ssid")->valuestring,sizeof(wifi_config.sta.ssid));
    strncpy((char *)&wifi_config.sta.password, cJSON_GetObjectItemCaseSensitive(wifi, "pwd")->valuestring,sizeof(wifi_config.sta.password));

    return wifi_sta_do_connect(wifi_config, true);
}

