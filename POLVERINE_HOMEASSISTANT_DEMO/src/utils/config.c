/**
 * @file config.c
 * @brief Configuration management implementation
 */

#include "config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "config";

// NVS namespace for configuration
#define CONFIG_NAMESPACE "polverine_cfg"

// NVS keys
#define KEY_WIFI_SSID     "wifi_ssid"
#define KEY_WIFI_PASS     "wifi_pass"
#define KEY_MQTT_URI      "mqtt_uri"
#define KEY_MQTT_USER     "mqtt_user"
#define KEY_MQTT_PASS     "mqtt_pass"
#define KEY_MQTT_CLIENT   "mqtt_client"

// Default values (can be overridden at compile time)
#ifndef DEFAULT_WIFI_SSID
#define DEFAULT_WIFI_SSID ""
#endif

#ifndef DEFAULT_WIFI_PASS
#define DEFAULT_WIFI_PASS ""
#endif

#ifndef DEFAULT_MQTT_URI
#define DEFAULT_MQTT_URI "mqtt://homeassistant.local"
#endif

#ifndef DEFAULT_MQTT_USER
#define DEFAULT_MQTT_USER ""
#endif

#ifndef DEFAULT_MQTT_PASS
#define DEFAULT_MQTT_PASS ""
#endif

static nvs_handle_t config_handle = 0;

bool config_init(void)
{
    esp_err_t err = nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &config_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "Configuration system initialized");
    return true;
}

static bool load_string_from_nvs(const char *key, char *value, size_t max_len, const char *default_value)
{
    size_t length = max_len;

    /* Always leave *value* a valid, NUL-terminated string. Callers such as
     * config_load_mqtt() ignore the return value for optional fields
     * (username/password), so bailing out without touching the buffer left
     * them holding uninitialised stack bytes, which were then handed to the
     * MQTT client as credentials and broke the broker connection for any
     * anonymous broker. */
    if (max_len > 0) {
        value[0] = '\0';
    }

    esp_err_t err = nvs_get_str(config_handle, key, value, &length);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded %s from NVS", key);
        return true;
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        // Use default value
        if (default_value && strlen(default_value) > 0) {
            strncpy(value, default_value, max_len - 1);
            value[max_len - 1] = '\0';
            ESP_LOGI(TAG, "Using default value for %s", key);
            return true;
        }
        /* No stored value and no non-empty default: an empty string is the
         * correct result for optional fields (e.g. anonymous MQTT). The
         * buffer was emptied above; report false so callers that need a
         * generated value (client_id) still know nothing was loaded. */
        ESP_LOGI(TAG, "%s not set; using empty value", key);
        return false;
    } else {
        ESP_LOGE(TAG, "Failed to read %s: %s", key, esp_err_to_name(err));
        return false;
    }
}

static bool save_string_to_nvs(const char *key, const char *value)
{
    esp_err_t err = nvs_set_str(config_handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save %s: %s", key, esp_err_to_name(err));
        return false;
    }
    
    err = nvs_commit(config_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit %s: %s", key, esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "Saved %s to NVS", key);
    return true;
}

bool config_load_wifi(polverine_wifi_config_t *config)
{
    if (!config || !config_handle) {
        return false;
    }
    
    bool ssid_loaded = load_string_from_nvs(KEY_WIFI_SSID, config->ssid, 
                                            sizeof(config->ssid), DEFAULT_WIFI_SSID);
    bool pass_loaded = load_string_from_nvs(KEY_WIFI_PASS, config->password, 
                                            sizeof(config->password), DEFAULT_WIFI_PASS);
    
    if (!ssid_loaded || strlen(config->ssid) == 0) {
        ESP_LOGE(TAG, "WiFi SSID not configured");
        return false;
    }
    
    ESP_LOGI(TAG, "WiFi configuration loaded: SSID=%s", config->ssid);
    return true;
}

bool config_save_wifi(const polverine_wifi_config_t *config)
{
    if (!config || !config_handle) {
        return false;
    }
    
    bool success = save_string_to_nvs(KEY_WIFI_SSID, config->ssid) &&
                   save_string_to_nvs(KEY_WIFI_PASS, config->password);
    
    if (success) {
        ESP_LOGI(TAG, "WiFi configuration saved");
    }
    
    return success;
}

bool config_load_mqtt(polverine_mqtt_config_t *config, const char *device_id)
{
    if (!config || !config_handle) {
        return false;
    }
    
    // Load URI
    if (!load_string_from_nvs(KEY_MQTT_URI, config->uri, 
                             sizeof(config->uri), DEFAULT_MQTT_URI)) {
        strcpy(config->uri, DEFAULT_MQTT_URI);
    }
    
    // Load username
    load_string_from_nvs(KEY_MQTT_USER, config->username, 
                        sizeof(config->username), DEFAULT_MQTT_USER);
    
    // Load password
    load_string_from_nvs(KEY_MQTT_PASS, config->password, 
                        sizeof(config->password), DEFAULT_MQTT_PASS);
    
    // Load or generate client ID
    if (!load_string_from_nvs(KEY_MQTT_CLIENT, config->client_id, 
                             sizeof(config->client_id), NULL)) {
        // Generate client ID from device ID
        snprintf(config->client_id, sizeof(config->client_id), 
                 "polverine_%s", device_id ? device_id : "unknown");
    }
    
    ESP_LOGI(TAG, "MQTT configuration loaded: URI=%s, ClientID=%s", 
             config->uri, config->client_id);
    return true;
}

bool config_save_mqtt(const polverine_mqtt_config_t *config)
{
    if (!config || !config_handle) {
        return false;
    }
    
    bool success = save_string_to_nvs(KEY_MQTT_URI, config->uri) &&
                   save_string_to_nvs(KEY_MQTT_USER, config->username) &&
                   save_string_to_nvs(KEY_MQTT_PASS, config->password) &&
                   save_string_to_nvs(KEY_MQTT_CLIENT, config->client_id);
    
    if (success) {
        ESP_LOGI(TAG, "MQTT configuration saved");
    }
    
    return success;
}

bool config_clear_all(void)
{
    if (!config_handle) {
        return false;
    }
    
    esp_err_t err = nvs_erase_all(config_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase configuration: %s", esp_err_to_name(err));
        return false;
    }
    
    err = nvs_commit(config_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit erase: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "All configuration cleared");
    return true;
}