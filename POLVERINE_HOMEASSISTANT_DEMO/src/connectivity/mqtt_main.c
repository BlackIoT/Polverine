#include <stdio.h>
#include <string.h>
#include "esp_mac.h"
#include "peripherals.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "config.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/temperature_sensor.h"

const char *TAG = "mqtt_polverine_ha";

// Secrets moved to external configuration

// Home Assistant discovery topics
const char *TEMPLATE_HA_DISCOVERY_BME690_TEMP = "homeassistant/sensor/polverine_%s/temperature/config";
const char *TEMPLATE_HA_DISCOVERY_BME690_HUMID = "homeassistant/sensor/polverine_%s/humidity/config";
const char *TEMPLATE_HA_DISCOVERY_BME690_PRESS = "homeassistant/sensor/polverine_%s/pressure/config";
const char *TEMPLATE_HA_DISCOVERY_BME690_IAQ = "homeassistant/sensor/polverine_%s/iaq/config";
const char *TEMPLATE_HA_DISCOVERY_BME690_CO2 = "homeassistant/sensor/polverine_%s/co2/config";
const char *TEMPLATE_HA_DISCOVERY_BME690_VOC = "homeassistant/sensor/polverine_%s/voc/config";
const char *TEMPLATE_HA_DISCOVERY_BME690_IAQ_ACC = "homeassistant/sensor/polverine_%s/iaq_accuracy/config";
const char *TEMPLATE_HA_DISCOVERY_BME690_STATIC_IAQ = "homeassistant/sensor/polverine_%s/static_iaq/config";
const char *TEMPLATE_HA_DISCOVERY_BME690_GAS_PERCENTAGE = "homeassistant/sensor/polverine_%s/gas_percentage/config";
const char *TEMPLATE_HA_DISCOVERY_BME690_STABILIZATION = "homeassistant/binary_sensor/polverine_%s/stabilization_status/config";
const char *TEMPLATE_HA_DISCOVERY_BME690_RUN_IN = "homeassistant/binary_sensor/polverine_%s/run_in_status/config";

const char *TEMPLATE_HA_DISCOVERY_BMV080_PM10 = "homeassistant/sensor/polverine_%s/pm10/config";
const char *TEMPLATE_HA_DISCOVERY_BMV080_PM25 = "homeassistant/sensor/polverine_%s/pm25/config";
const char *TEMPLATE_HA_DISCOVERY_BMV080_PM1 = "homeassistant/sensor/polverine_%s/pm1/config";
const char *TEMPLATE_HA_DISCOVERY_BMV080_OBSTRUCTED = "homeassistant/binary_sensor/polverine_%s/obstructed/config";
const char *TEMPLATE_HA_DISCOVERY_BMV080_OUT_OF_RANGE = "homeassistant/binary_sensor/polverine_%s/out_of_range/config";

// ESP32 system metrics discovery topics
const char *TEMPLATE_HA_DISCOVERY_ESP32_RSSI = "homeassistant/sensor/polverine_%s/wifi_rssi/config";
const char *TEMPLATE_HA_DISCOVERY_ESP32_HEAP = "homeassistant/sensor/polverine_%s/free_heap/config";
const char *TEMPLATE_HA_DISCOVERY_ESP32_UPTIME = "homeassistant/sensor/polverine_%s/uptime/config";
const char *TEMPLATE_HA_DISCOVERY_ESP32_TEMP = "homeassistant/sensor/polverine_%s/cpu_temperature/config";

// Home Assistant state topics
const char *TEMPLATE_HA_STATE_BME690 = "polverine/%s/bme690/state";
const char *TEMPLATE_HA_STATE_BMV080 = "polverine/%s/bmv080/state";
const char *TEMPLATE_HA_STATE_SYSTEM = "polverine/%s/system/state";
const char *TEMPLATE_HA_AVAILABILITY = "polverine/%s/availability";

// Configuration loaded from NVS

cJSON *mqtt = 0;

extern char shortId[7];
static char device_name[64];
static char availability_topic[128];
static char bme690_state_topic[128];
static char bmv080_state_topic[128];
static char system_state_topic[128];

void mqtt_default_init(const char *id)
{
    snprintf(device_name, sizeof(device_name), "Polverine %s", id);
    snprintf(availability_topic, sizeof(availability_topic), TEMPLATE_HA_AVAILABILITY, id);
    snprintf(bme690_state_topic, sizeof(bme690_state_topic), TEMPLATE_HA_STATE_BME690, id);
    snprintf(bmv080_state_topic, sizeof(bmv080_state_topic), TEMPLATE_HA_STATE_BMV080, id);
    snprintf(system_state_topic, sizeof(system_state_topic), TEMPLATE_HA_STATE_SYSTEM, id);
}

void mqtt_store_read(void)
{
    polverine_mqtt_config_t config;
    
    if (config_load_mqtt(&config, shortId)) {
        // Create JSON from loaded config
        if(mqtt) {
            cJSON_Delete(mqtt);
            mqtt = 0;
        }
        
        mqtt = cJSON_CreateObject();
        cJSON_AddStringToObject(mqtt, "uri", config.uri);
        cJSON_AddStringToObject(mqtt, "user", config.username);
        cJSON_AddStringToObject(mqtt, "pwd", config.password);
        cJSON_AddStringToObject(mqtt, "clientid", config.client_id);
        
        ESP_LOGI(TAG, "MQTT configuration loaded: URI=%s, ClientID=%s", 
                 config.uri, config.client_id);
    } else {
        ESP_LOGE(TAG, "Failed to load MQTT configuration");
    }
}

void mqtt_store_write(const char *buffer)
{
    cJSON *json = cJSON_Parse(buffer);
    
    if(json) {
        polverine_mqtt_config_t config = {0};
        
        cJSON *uri = cJSON_GetObjectItemCaseSensitive(json, "uri");
        cJSON *user = cJSON_GetObjectItemCaseSensitive(json, "user");
        cJSON *pwd = cJSON_GetObjectItemCaseSensitive(json, "pwd");
        cJSON *clientid = cJSON_GetObjectItemCaseSensitive(json, "clientid");
        
        if (uri && cJSON_IsString(uri)) {
            strncpy(config.uri, uri->valuestring, sizeof(config.uri) - 1);
        }
        if (user && cJSON_IsString(user)) {
            strncpy(config.username, user->valuestring, sizeof(config.username) - 1);
        }
        if (pwd && cJSON_IsString(pwd)) {
            strncpy(config.password, pwd->valuestring, sizeof(config.password) - 1);
        }
        if (clientid && cJSON_IsString(clientid)) {
            strncpy(config.client_id, clientid->valuestring, sizeof(config.client_id) - 1);
        }
        
        if (config_save_mqtt(&config)) {
            ESP_LOGI(TAG, "MQTT configuration saved");
        } else {
            ESP_LOGE(TAG, "Failed to save MQTT configuration");
        }
        
        cJSON_Delete(json);
    }
}

// Add a dummy topic_store_write function to maintain compatibility
void topic_store_write(const char *buffer)
{
    // In Home Assistant mode, topics are fixed based on HA discovery
    // This function is kept for compatibility but doesn't do anything
    ESP_LOGW(TAG, "topic_store_write called but topics are managed by Home Assistant discovery");
}

bool isConnected = false;
esp_mqtt_client_handle_t client = 0;
#define MQTT_CONNECTION_TIMEOUT_MB 15000
TickType_t mqtt_connection_start_time = 0;

// Send Home Assistant discovery messages
static void send_ha_discovery()
{
    char topic[256];
    char payload[1024];
    
    // Create device object for all sensors
    cJSON *device = cJSON_CreateObject();
    cJSON_AddStringToObject(device, "identifiers", shortId);
    cJSON_AddStringToObject(device, "name", device_name);
    cJSON_AddStringToObject(device, "manufacturer", "BlackIoT");
    cJSON_AddStringToObject(device, "model", "Polverine Sensor");
    char *device_str = cJSON_PrintUnformatted(device);
    
    // BME690 Temperature
    snprintf(topic, sizeof(topic), TEMPLATE_HA_DISCOVERY_BME690_TEMP, shortId);
    snprintf(payload, sizeof(payload), 
        "{\"unique_id\":\"%s_temperature\","
        "\"name\":\"Temperature\","
        "\"state_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"device_class\":\"temperature\","
        "\"unit_of_measurement\":\"°C\","
        "\"value_template\":\"{{ value_json.temperature }}\","
        "\"device\":%s}",
        shortId, bme690_state_topic, availability_topic, device_str);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    
    // BME690 Humidity
    snprintf(topic, sizeof(topic), TEMPLATE_HA_DISCOVERY_BME690_HUMID, shortId);
    snprintf(payload, sizeof(payload), 
        "{\"unique_id\":\"%s_humidity\","
        "\"name\":\"Humidity\","
        "\"state_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"device_class\":\"humidity\","
        "\"unit_of_measurement\":\"%%\","
        "\"value_template\":\"{{ value_json.humidity }}\","
        "\"device\":%s}",
        shortId, bme690_state_topic, availability_topic, device_str);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    
    // BME690 Pressure
    snprintf(topic, sizeof(topic), TEMPLATE_HA_DISCOVERY_BME690_PRESS, shortId);
    snprintf(payload, sizeof(payload), 
        "{\"unique_id\":\"%s_pressure\","
        "\"name\":\"Pressure\","
        "\"state_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"device_class\":\"pressure\","
        "\"unit_of_measurement\":\"hPa\","
        "\"value_template\":\"{{ value_json.pressure | float / 100 }}\","
        "\"device\":%s}",
        shortId, bme690_state_topic, availability_topic, device_str);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    
    // BME690 IAQ
    snprintf(topic, sizeof(topic), TEMPLATE_HA_DISCOVERY_BME690_IAQ, shortId);
    snprintf(payload, sizeof(payload), 
        "{\"unique_id\":\"%s_iaq\","
        "\"name\":\"Indoor Air Quality\","
        "\"state_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"unit_of_measurement\":\"IAQ\","
        "\"value_template\":\"{{ value_json.iaq }}\","
        "\"device\":%s}",
        shortId, bme690_state_topic, availability_topic, device_str);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    
    // BME690 CO2
    snprintf(topic, sizeof(topic), TEMPLATE_HA_DISCOVERY_BME690_CO2, shortId);
    snprintf(payload, sizeof(payload), 
        "{\"unique_id\":\"%s_co2\","
        "\"name\":\"CO2\","
        "\"state_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"device_class\":\"carbon_dioxide\","
        "\"unit_of_measurement\":\"ppm\","
        "\"value_template\":\"{{ value_json.co2 }}\","
        "\"device\":%s}",
        shortId, bme690_state_topic, availability_topic, device_str);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    
    // BME690 VOC
    snprintf(topic, sizeof(topic), TEMPLATE_HA_DISCOVERY_BME690_VOC, shortId);
    snprintf(payload, sizeof(payload), 
        "{\"unique_id\":\"%s_voc\","
        "\"name\":\"VOC\","
        "\"state_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"device_class\":\"volatile_organic_compounds_parts\","
        "\"unit_of_measurement\":\"ppm\","
        "\"value_template\":\"{{ value_json.voc }}\","
        "\"device\":%s}",
        shortId, bme690_state_topic, availability_topic, device_str);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    
    // BMV080 PM10
    snprintf(topic, sizeof(topic), TEMPLATE_HA_DISCOVERY_BMV080_PM10, shortId);
    snprintf(payload, sizeof(payload), 
        "{\"unique_id\":\"%s_pm10\","
        "\"name\":\"PM10\","
        "\"state_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"device_class\":\"pm10\","
        "\"unit_of_measurement\":\"µg/m³\","
        "\"value_template\":\"{{ value_json.pm10 }}\","
        "\"device\":%s}",
        shortId, bmv080_state_topic, availability_topic, device_str);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    
    // BMV080 PM2.5
    snprintf(topic, sizeof(topic), TEMPLATE_HA_DISCOVERY_BMV080_PM25, shortId);
    snprintf(payload, sizeof(payload), 
        "{\"unique_id\":\"%s_pm25\","
        "\"name\":\"PM2.5\","
        "\"state_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"device_class\":\"pm25\","
        "\"unit_of_measurement\":\"µg/m³\","
        "\"value_template\":\"{{ value_json.pm25 }}\","
        "\"device\":%s}",
        shortId, bmv080_state_topic, availability_topic, device_str);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    
    // BMV080 PM1
    snprintf(topic, sizeof(topic), TEMPLATE_HA_DISCOVERY_BMV080_PM1, shortId);
    snprintf(payload, sizeof(payload), 
        "{\"unique_id\":\"%s_pm1\","
        "\"name\":\"PM1\","
        "\"state_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"device_class\":\"pm1\","
        "\"unit_of_measurement\":\"µg/m³\","
        "\"value_template\":\"{{ value_json.pm1 }}\","
        "\"device\":%s}",
        shortId, bmv080_state_topic, availability_topic, device_str);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    
    // BME690 IAQ Accuracy
    snprintf(topic, sizeof(topic), TEMPLATE_HA_DISCOVERY_BME690_IAQ_ACC, shortId);
    snprintf(payload, sizeof(payload), 
        "{\"unique_id\":\"%s_iaq_accuracy\","
        "\"name\":\"IAQ Accuracy\","
        "\"state_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"value_template\":\"{{ value_json.iaq_accuracy }}\","
        "\"icon\":\"mdi:gauge\","
        "\"state_class\":\"measurement\","
        "\"device\":%s}",
        shortId, bme690_state_topic, availability_topic, device_str);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    
    // BME690 Static IAQ
    snprintf(topic, sizeof(topic), TEMPLATE_HA_DISCOVERY_BME690_STATIC_IAQ, shortId);
    snprintf(payload, sizeof(payload), 
        "{\"unique_id\":\"%s_static_iaq\","
        "\"name\":\"Static IAQ\","
        "\"state_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"value_template\":\"{{ value_json.static_iaq }}\","
        "\"icon\":\"mdi:air-filter\","
        "\"state_class\":\"measurement\","
        "\"device\":%s}",
        shortId, bme690_state_topic, availability_topic, device_str);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    
    // BME690 Gas Percentage
    snprintf(topic, sizeof(topic), TEMPLATE_HA_DISCOVERY_BME690_GAS_PERCENTAGE, shortId);
    snprintf(payload, sizeof(payload), 
        "{\"unique_id\":\"%s_gas_percentage\","
        "\"name\":\"Gas Percentage\","
        "\"state_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"unit_of_measurement\":\"%%\","
        "\"value_template\":\"{{ value_json.gas_percentage }}\","
        "\"icon\":\"mdi:percent\","
        "\"state_class\":\"measurement\","
        "\"device\":%s}",
        shortId, bme690_state_topic, availability_topic, device_str);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    
    // BME690 Stabilization Status
    snprintf(topic, sizeof(topic), TEMPLATE_HA_DISCOVERY_BME690_STABILIZATION, shortId);
    snprintf(payload, sizeof(payload), 
        "{\"unique_id\":\"%s_stabilization_status\","
        "\"name\":\"Gas Sensor Stabilized\","
        "\"state_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"payload_on\":\"true\","
        "\"payload_off\":\"false\","
        "\"value_template\":\"{{ value_json.stabilization_status }}\","
        "\"icon\":\"mdi:check-circle\","
        "\"device\":%s}",
        shortId, bme690_state_topic, availability_topic, device_str);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    
    // BME690 Run-in Status
    snprintf(topic, sizeof(topic), TEMPLATE_HA_DISCOVERY_BME690_RUN_IN, shortId);
    snprintf(payload, sizeof(payload), 
        "{\"unique_id\":\"%s_run_in_status\","
        "\"name\":\"Gas Sensor Run-in Complete\","
        "\"state_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"payload_on\":\"true\","
        "\"payload_off\":\"false\","
        "\"value_template\":\"{{ value_json.run_in_status }}\","
        "\"icon\":\"mdi:timer-check\","
        "\"device\":%s}",
        shortId, bme690_state_topic, availability_topic, device_str);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    
    // BMV080 Obstructed
    snprintf(topic, sizeof(topic), TEMPLATE_HA_DISCOVERY_BMV080_OBSTRUCTED, shortId);
    snprintf(payload, sizeof(payload), 
        "{\"unique_id\":\"%s_obstructed\","
        "\"name\":\"Sensor Obstructed\","
        "\"state_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"payload_on\":\"true\","
        "\"payload_off\":\"false\","
        "\"value_template\":\"{{ value_json.obstructed }}\","
        "\"device_class\":\"problem\","
        "\"device\":%s}",
        shortId, bmv080_state_topic, availability_topic, device_str);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    
    // BMV080 Out of Range
    snprintf(topic, sizeof(topic), TEMPLATE_HA_DISCOVERY_BMV080_OUT_OF_RANGE, shortId);
    snprintf(payload, sizeof(payload), 
        "{\"unique_id\":\"%s_out_of_range\","
        "\"name\":\"Measurement Out of Range\","
        "\"state_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"payload_on\":\"true\","
        "\"payload_off\":\"false\","
        "\"value_template\":\"{{ value_json.out_of_range }}\","
        "\"device_class\":\"problem\","
        "\"device\":%s}",
        shortId, bmv080_state_topic, availability_topic, device_str);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    
    // ESP32 WiFi RSSI
    snprintf(topic, sizeof(topic), TEMPLATE_HA_DISCOVERY_ESP32_RSSI, shortId);
    snprintf(payload, sizeof(payload), 
        "{\"unique_id\":\"%s_wifi_rssi\","
        "\"name\":\"WiFi Signal\","
        "\"state_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"device_class\":\"signal_strength\","
        "\"unit_of_measurement\":\"dBm\","
        "\"value_template\":\"{{ value_json.rssi }}\","
        "\"icon\":\"mdi:wifi\","
        "\"device\":%s}",
        shortId, system_state_topic, availability_topic, device_str);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    
    // ESP32 Free Heap
    snprintf(topic, sizeof(topic), TEMPLATE_HA_DISCOVERY_ESP32_HEAP, shortId);
    snprintf(payload, sizeof(payload), 
        "{\"unique_id\":\"%s_free_heap\","
        "\"name\":\"Free Memory\","
        "\"state_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"unit_of_measurement\":\"B\","
        "\"value_template\":\"{{ value_json.free_heap }}\","
        "\"icon\":\"mdi:memory\","
        "\"state_class\":\"measurement\","
        "\"device\":%s}",
        shortId, system_state_topic, availability_topic, device_str);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    
    // ESP32 Uptime
    snprintf(topic, sizeof(topic), TEMPLATE_HA_DISCOVERY_ESP32_UPTIME, shortId);
    snprintf(payload, sizeof(payload), 
        "{\"unique_id\":\"%s_uptime\","
        "\"name\":\"Uptime\","
        "\"state_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"unit_of_measurement\":\"s\","
        "\"value_template\":\"{{ value_json.uptime }}\","
        "\"icon\":\"mdi:timer-outline\","
        "\"state_class\":\"total_increasing\","
        "\"device\":%s}",
        shortId, system_state_topic, availability_topic, device_str);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    
    // ESP32 CPU Temperature
    snprintf(topic, sizeof(topic), TEMPLATE_HA_DISCOVERY_ESP32_TEMP, shortId);
    snprintf(payload, sizeof(payload), 
        "{\"unique_id\":\"%s_cpu_temperature\","
        "\"name\":\"CPU Temperature\","
        "\"state_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"device_class\":\"temperature\","
        "\"unit_of_measurement\":\"°C\","
        "\"value_template\":\"{{ value_json.cpu_temp }}\","
        "\"icon\":\"mdi:thermometer\","
        "\"state_class\":\"measurement\","
        "\"device\":%s}",
        shortId, system_state_topic, availability_topic, device_str);
    esp_mqtt_client_publish(client, topic, payload, 0, 1, true);
    
    cJSON_Delete(device);
    free(device_str);
    
    ESP_LOGI(TAG, "Home Assistant discovery messages sent");
}

// Publish BME690 data with Home Assistant format
void bme690_publish(const char *buffer)
{
    if(!isConnected) return;
    
    // Parse the existing JSON
    cJSON *json = cJSON_Parse(buffer);
    if(!json) return;
    
    // Create new JSON for Home Assistant
    cJSON *ha_json = cJSON_CreateObject();
    
    // Get values from original JSON
    cJSON *temp = cJSON_GetObjectItemCaseSensitive(json, "T");
    cJSON *hum = cJSON_GetObjectItemCaseSensitive(json, "H");
    cJSON *press = cJSON_GetObjectItemCaseSensitive(json, "P");
    cJSON *iaq = cJSON_GetObjectItemCaseSensitive(json, "IAQ");
    cJSON *co2 = cJSON_GetObjectItemCaseSensitive(json, "CO2");
    cJSON *voc = cJSON_GetObjectItemCaseSensitive(json, "VOC");
    cJSON *acc = cJSON_GetObjectItemCaseSensitive(json, "ACC");
    cJSON *siaq = cJSON_GetObjectItemCaseSensitive(json, "SIAQ");
    cJSON *gasp = cJSON_GetObjectItemCaseSensitive(json, "GASP");
    cJSON *stab = cJSON_GetObjectItemCaseSensitive(json, "STAB");
    cJSON *run = cJSON_GetObjectItemCaseSensitive(json, "RUN");
    cJSON *mtof = cJSON_GetObjectItemCaseSensitive(json, "mtof");
    
    // Add to HA JSON
    if(temp) cJSON_AddNumberToObject(ha_json, "temperature", temp->valuedouble);
    if(hum) cJSON_AddNumberToObject(ha_json, "humidity", hum->valuedouble);
    if(press) cJSON_AddNumberToObject(ha_json, "pressure", press->valuedouble);
    if(iaq) cJSON_AddNumberToObject(ha_json, "iaq", iaq->valuedouble);
    if(co2) cJSON_AddNumberToObject(ha_json, "co2", co2->valuedouble);
    if(voc) cJSON_AddNumberToObject(ha_json, "voc", voc->valuedouble);
    if(acc) cJSON_AddNumberToObject(ha_json, "iaq_accuracy", acc->valuedouble);
    if(siaq) cJSON_AddNumberToObject(ha_json, "static_iaq", siaq->valuedouble);
    if(gasp) cJSON_AddNumberToObject(ha_json, "gas_percentage", gasp->valuedouble);
    if(stab) cJSON_AddStringToObject(ha_json, "stabilization_status", stab->valuedouble > 0.5 ? "true" : "false");
    if(run) cJSON_AddStringToObject(ha_json, "run_in_status", run->valuedouble > 0.5 ? "true" : "false");
    if(mtof) cJSON_AddNumberToObject(ha_json, "mtof", mtof->valuedouble);
    
    char *ha_payload = cJSON_PrintUnformatted(ha_json);
    esp_mqtt_client_publish(client, bme690_state_topic, ha_payload, 0, 1, 0);  // QoS 1 for reliability
    
    cJSON_Delete(json);
    cJSON_Delete(ha_json);
    free(ha_payload);
}

// Publish BMV080 data with Home Assistant format
void bmv080_publish(const char *buffer)
{
    if(!isConnected) return;
    
    // Parse the existing JSON
    cJSON *json = cJSON_Parse(buffer);
    if(!json) return;
    
    // Create new JSON for Home Assistant
    cJSON *ha_json = cJSON_CreateObject();
    
    // Get values from original JSON
    cJSON *pm10 = cJSON_GetObjectItemCaseSensitive(json, "PM10");
    cJSON *pm25 = cJSON_GetObjectItemCaseSensitive(json, "PM25");
    cJSON *pm1 = cJSON_GetObjectItemCaseSensitive(json, "PM1");
    cJSON *obst = cJSON_GetObjectItemCaseSensitive(json, "obst");
    cJSON *omr = cJSON_GetObjectItemCaseSensitive(json, "omr");
    
    // Add to HA JSON
    if(pm10) cJSON_AddNumberToObject(ha_json, "pm10", pm10->valuedouble);
    if(pm25) cJSON_AddNumberToObject(ha_json, "pm25", pm25->valuedouble);
    if(pm1) cJSON_AddNumberToObject(ha_json, "pm1", pm1->valuedouble);
    
    // Add boolean values for obstructed and out of range
    if(obst && cJSON_IsString(obst)) {
        cJSON_AddStringToObject(ha_json, "obstructed", strcmp(obst->valuestring, "yes") == 0 ? "true" : "false");
    }
    if(omr && cJSON_IsString(omr)) {
        cJSON_AddStringToObject(ha_json, "out_of_range", strcmp(omr->valuestring, "yes") == 0 ? "true" : "false");
    }
    
    char *ha_payload = cJSON_PrintUnformatted(ha_json);
    esp_mqtt_client_publish(client, bmv080_state_topic, ha_payload, 0, 1, 0);  // QoS 1 for reliability
    
    cJSON_Delete(json);
    cJSON_Delete(ha_json);
    free(ha_payload);
}

// Forward declarations
static void system_metrics_task(void *pvParameter);
void mqtt_start_system_metrics_task(void);
static TaskHandle_t system_metrics_task_handle = NULL;

// Publish ESP32 system metrics
void system_metrics_publish(void)
{
    if(!isConnected) return;
    
    // Create JSON for system metrics
    cJSON *sys_json = cJSON_CreateObject();
    
    // Get WiFi RSSI
    wifi_ap_record_t ap_info;
    if(esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        cJSON_AddNumberToObject(sys_json, "rssi", ap_info.rssi);
    }
    
    // Get free heap
    cJSON_AddNumberToObject(sys_json, "free_heap", esp_get_free_heap_size());
    
    // Get uptime in seconds
    cJSON_AddNumberToObject(sys_json, "uptime", xTaskGetTickCount() * portTICK_PERIOD_MS / 1000);
    
    // Get CPU temperature
    static temperature_sensor_handle_t temp_handle = NULL;
    if(temp_handle == NULL) {
        temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
        temperature_sensor_install(&temp_sensor_config, &temp_handle);
        temperature_sensor_enable(temp_handle);
    }
    
    float cpu_temp;
    if(temperature_sensor_get_celsius(temp_handle, &cpu_temp) == ESP_OK) {
        cJSON_AddNumberToObject(sys_json, "cpu_temp", cpu_temp);
    }
    
    char *sys_payload = cJSON_PrintUnformatted(sys_json);
    esp_mqtt_client_publish(client, system_state_topic, sys_payload, 0, 1, 0);  // QoS 1 for reliability
    
    cJSON_Delete(sys_json);
    free(sys_payload);
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    client = event->client;
    
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        isConnected = true;
        mqtt_connection_start_time = 0;
        
        // Publish online status FIRST with QoS 1 and retain
        int msg_id = esp_mqtt_client_publish(client, availability_topic, "online", 0, 1, true);
        ESP_LOGI(TAG, "Published availability 'online' to %s, msg_id=%d", availability_topic, msg_id);
        
        // Wait a bit to ensure availability is published before discovery
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Send Home Assistant discovery messages
        send_ha_discovery();
        break;
        
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        isConnected = false;
        
        // Attempt to reconnect with shorter delay for better availability
        ESP_LOGI(TAG, "Will attempt to reconnect in 2 seconds...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        // Get MQTT configuration for detailed logging
        cJSON *uri_item = cJSON_GetObjectItemCaseSensitive(mqtt, "uri");
        cJSON *user_item = cJSON_GetObjectItemCaseSensitive(mqtt, "user");
        cJSON *pwd_item = cJSON_GetObjectItemCaseSensitive(mqtt, "pwd");
        cJSON *clientid_item = cJSON_GetObjectItemCaseSensitive(mqtt, "clientid");
        
        if (uri_item && user_item && pwd_item && clientid_item) {
            ESP_LOGI(TAG, "Attempting to reconnect to MQTT broker...");
            ESP_LOGI(TAG, "  Broker URI: %s", uri_item->valuestring);
            ESP_LOGI(TAG, "  Username: %s", user_item->valuestring);
            ESP_LOGI(TAG, "  Password: %s", pwd_item->valuestring);
            ESP_LOGI(TAG, "  Client ID: %s", clientid_item->valuestring);
            
            // Parse URI to show host and port separately
            const char *uri = uri_item->valuestring;
            if (strncmp(uri, "mqtt://", 7) == 0) {
                const char *host_port = uri + 7;
                char *colon = strchr(host_port, ':');
                if (colon) {
                    char host[256];
                    int host_len = colon - host_port;
                    strncpy(host, host_port, host_len);
                    host[host_len] = '\0';
                    const char *port = colon + 1;
                    ESP_LOGI(TAG, "  Host: %s, Port: %s", host, port);
                } else {
                    ESP_LOGI(TAG, "  Host: %s, Port: 1883 (default)", host_port);
                }
            } else if (strncmp(uri, "mqtts://", 8) == 0) {
                const char *host_port = uri + 8;
                char *colon = strchr(host_port, ':');
                if (colon) {
                    char host[256];
                    int host_len = colon - host_port;
                    strncpy(host, host_port, host_len);
                    host[host_len] = '\0';
                    const char *port = colon + 1;
                    ESP_LOGI(TAG, "  Host: %s, Port: %s (TLS)", host, port);
                } else {
                    ESP_LOGI(TAG, "  Host: %s, Port: 8883 (default TLS)", host_port);
                }
            } else {
                ESP_LOGI(TAG, "  URI format: %s", uri);
            }
        } else {
            ESP_LOGE(TAG, "MQTT configuration incomplete, cannot log connection details");
        }
        
        esp_err_t err = esp_mqtt_client_reconnect(client);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initiate reconnection: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Reconnection initiated");
            mqtt_connection_start_time = xTaskGetTickCount();
        }
        break;
        
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
        
    case MQTT_EVENT_DATA:
        ESP_LOGD(TAG, "MQTT_EVENT_DATA");
        ESP_LOGD(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
        ESP_LOGD(TAG, "DATA=%.*s", event->data_len, event->data);
        break;
        
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_app_start(void)
{
    ESP_LOGI(TAG, "Starting Home Assistant MQTT application...");
    
    mqtt_store_read();
    
    // Configure MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {0};
    mqtt_cfg.broker.address.uri = cJSON_GetObjectItemCaseSensitive(mqtt, "uri")->valuestring;
    mqtt_cfg.credentials.username = cJSON_GetObjectItemCaseSensitive(mqtt, "user")->valuestring;
    mqtt_cfg.credentials.authentication.password = cJSON_GetObjectItemCaseSensitive(mqtt, "pwd")->valuestring;
    mqtt_cfg.credentials.client_id = cJSON_GetObjectItemCaseSensitive(mqtt, "clientid")->valuestring;
    
    // Set last will message for availability
    mqtt_cfg.session.last_will.topic = availability_topic;
    mqtt_cfg.session.last_will.msg = "offline";
    mqtt_cfg.session.last_will.qos = 1;
    mqtt_cfg.session.last_will.retain = true;
    
    mqtt_cfg.network.timeout_ms = 10000;
    mqtt_cfg.session.keepalive = 30;  // Increased for better stability
    
    // Log detailed connection information
    ESP_LOGI(TAG, "MQTT client configuration prepared:");
    ESP_LOGI(TAG, "  Broker URI: %s", mqtt_cfg.broker.address.uri);
    ESP_LOGI(TAG, "  Username: %s", mqtt_cfg.credentials.username);
    ESP_LOGI(TAG, "  Password: %s", mqtt_cfg.credentials.authentication.password);
    ESP_LOGI(TAG, "  Client ID: %s", mqtt_cfg.credentials.client_id);
    ESP_LOGI(TAG, "  Keepalive: %d seconds", mqtt_cfg.session.keepalive);
    ESP_LOGI(TAG, "  Timeout: %d ms", mqtt_cfg.network.timeout_ms);
    
    // Parse URI to show host and port separately
    const char *uri = mqtt_cfg.broker.address.uri;
    if (strncmp(uri, "mqtt://", 7) == 0) {
        const char *host_port = uri + 7;
        char *colon = strchr(host_port, ':');
        if (colon) {
            char host[256];
            int host_len = colon - host_port;
            strncpy(host, host_port, host_len);
            host[host_len] = '\0';
            const char *port = colon + 1;
            ESP_LOGI(TAG, "  Host: %s, Port: %s", host, port);
        } else {
            ESP_LOGI(TAG, "  Host: %s, Port: 1883 (default)", host_port);
        }
    } else if (strncmp(uri, "mqtts://", 8) == 0) {
        const char *host_port = uri + 8;
        char *colon = strchr(host_port, ':');
        if (colon) {
            char host[256];
            int host_len = colon - host_port;
            strncpy(host, host_port, host_len);
            host[host_len] = '\0';
            const char *port = colon + 1;
            ESP_LOGI(TAG, "  Host: %s, Port: %s (TLS)", host, port);
        } else {
            ESP_LOGI(TAG, "  Host: %s, Port: 8883 (default TLS)", host_port);
        }
    } else {
        ESP_LOGI(TAG, "  URI format: %s", uri);
    }
    
    client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client!");
        return;
    }
    
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    
    mqtt_connection_start_time = xTaskGetTickCount();
    
    ESP_LOGI(TAG, "Attempting initial connection to MQTT broker...");
    esp_err_t err = esp_mqtt_client_start(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        mqtt_connection_start_time = 0;
        return;
    }
    
    ESP_LOGI(TAG, "MQTT client started successfully");
    
    // Start system metrics reporting task
    mqtt_start_system_metrics_task();
    ESP_LOGI(TAG, "System metrics task started");
}

// Task to periodically publish system metrics and availability
static void system_metrics_task(void *pvParameter)
{
    const TickType_t xDelay = 30000 / portTICK_PERIOD_MS;  // Publish every 30 seconds
    const TickType_t xAvailabilityDelay = 60000 / portTICK_PERIOD_MS;  // Availability every 60 seconds
    TickType_t lastAvailabilityTime = 0;
    
    for(;;) {
        vTaskDelay(xDelay);
        
        if (isConnected) {
            // Publish system metrics
            system_metrics_publish();
            
            // Check if it's time to send availability heartbeat
            TickType_t currentTime = xTaskGetTickCount();
            if ((currentTime - lastAvailabilityTime) >= xAvailabilityDelay) {
                int msg_id = esp_mqtt_client_publish(client, availability_topic, "online", 0, 1, true);
                ESP_LOGI(TAG, "Availability heartbeat sent, msg_id=%d", msg_id);
                lastAvailabilityTime = currentTime;
            }
        }
    }
}

void mqtt_start_system_metrics_task(void)
{
    // Only create task if it doesn't already exist
    if (system_metrics_task_handle == NULL) {
        xTaskCreate(&system_metrics_task, "system_metrics_task", 4096, NULL, 5, &system_metrics_task_handle);
        ESP_LOGI(TAG, "System metrics task created");
    } else {
        ESP_LOGI(TAG, "System metrics task already exists");
    }
}