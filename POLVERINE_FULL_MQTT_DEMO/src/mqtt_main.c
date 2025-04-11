#include <stdio.h>
#include "esp_mac.h"
#include "peripherals.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "cJSON.h"

const char *TAG = "mqtt_polverine";

const char *TEMPLATE_POLVERINE_MQTT = "{\"uri\":\"mqtt://192.168.1.128\",\"user\":\"mqttmaster\",\"pwd\":\"mqttpassword\",\"clientid\":\"%s\"}"; // local MQTT broker 
const char *TEMPLATE_POLVERINE_TOPIC = "{\"bmv080\":\"polverine/%s/bmv080\",\"bme690\":\"polverine/%s/bme690\",\"cmd\":\"polverine/%s/cmd\"}";

#define DEFAULT_LEN 256
char *DEFAULT_POLVERINE_MQTT[DEFAULT_LEN] = {0};
char *DEFAULT_POLVERINE_TOPIC[DEFAULT_LEN] = {0};

cJSON *mqtt = 0;
cJSON *topic = 0;

void mqtt_default_init(const char *id)
{
    snprintf((char * __restrict__)DEFAULT_POLVERINE_MQTT,DEFAULT_LEN,TEMPLATE_POLVERINE_MQTT,id);
    //snprintf((char * __restrict__)DEFAULT_POLVERINE_MQTT,DEFAULT_LEN,TEMPLATE_POLVERINE_MQTT);
    snprintf((char * __restrict__)DEFAULT_POLVERINE_TOPIC,DEFAULT_LEN,TEMPLATE_POLVERINE_TOPIC,id,id,id);
}

void mqtt_store_read(void)
{
    
static char buffer[256] = {0};

    nvs_handle_t handle;
    esp_err_t err = nvs_open("MQTT", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        size_t length = sizeof(buffer);
        err = nvs_get_str(handle, "SERVER", buffer, &length);
        if (err == ESP_OK) {
            if(mqtt)
            {
                cJSON_Delete(mqtt);
                mqtt = 0;
            }
            mqtt = cJSON_Parse(buffer);        
        }
    nvs_close(handle);
    }

    if(!mqtt)
    {
        mqtt = cJSON_Parse((const char *)DEFAULT_POLVERINE_MQTT);        
    }
}

void mqtt_store_write(const char *buffer)
{
    cJSON *json = cJSON_Parse(buffer);

    if(json)
    {
        nvs_handle_t handle;
        esp_err_t err = nvs_open("MQTT", NVS_READWRITE, &handle);
        if (err == ESP_OK) {
            err = nvs_set_str(handle, "SERVER", cJSON_Print(json));
        nvs_close(handle);
        }
    }
}

void topic_store_read(void)
{
    
static char buffer[256] = {0};

    nvs_handle_t handle;
    esp_err_t err = nvs_open("MQTT", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        size_t length = sizeof(buffer);
        err = nvs_get_str(handle, "TOPIC", buffer, &length);
        if (err == ESP_OK) {
            if(topic)
            {
                cJSON_Delete(topic);
                topic = 0;
            }
            topic = cJSON_Parse(buffer);        
        }
    nvs_close(handle);
    }

    if(!topic)
    {
        topic = cJSON_Parse((const char *)DEFAULT_POLVERINE_TOPIC);        
    }
}

void topic_store_write(const char *buffer)
{
    cJSON *json = cJSON_Parse(buffer);

    if(json)
    {
        nvs_handle_t handle;
        esp_err_t err = nvs_open("MQTT", NVS_READWRITE, &handle);
        if (err == ESP_OK) {
            err = nvs_set_str(handle, "TOPIC", cJSON_Print(json));
        nvs_close(handle);
        }
    }
}

bool isConnected = false;
esp_mqtt_client_handle_t client = 0;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

void bmv080_publish(const char *buffer)
{
  if(isConnected)
  {
    int msg_id = esp_mqtt_client_publish(client, cJSON_GetObjectItemCaseSensitive(topic, "bmv080")->valuestring, buffer, 0, 1, 0);        
  }
}

void bme690_publish(const char *buffer)
{
  if(isConnected)
  {
    int msg_id = esp_mqtt_client_publish(client, cJSON_GetObjectItemCaseSensitive(topic, "bme690")->valuestring, buffer, strlen(buffer), 0, 0);        
  }
}


/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        isConnected = true;
        //msg_id = esp_mqtt_client_publish(client, "polverine/bmv080", "data_3", 0, 1, 0);
        //ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, (char *)cJSON_GetObjectItemCaseSensitive(topic, "cmd")->valuestring, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}


void mqtt_app_start(void)
{
    //mqtt_store_write(DEFAULT_POLVERINE_MQTT);
    //topic_store_write(DEFAULT_POLVERINE_TOPIC);
    mqtt_store_read();
    topic_store_read();

    printf("[MQTT] MQTT=%s\r\n", cJSON_Print(mqtt));
    printf("[MQTT] TOPIC=%s\r\n", cJSON_Print(topic));

    esp_mqtt_client_config_t mqtt_cfg = {0};

    mqtt_cfg.broker.address.uri = cJSON_GetObjectItemCaseSensitive(mqtt, "uri")->valuestring;
    mqtt_cfg.credentials.username = cJSON_GetObjectItemCaseSensitive(mqtt, "user")->valuestring;
    mqtt_cfg.credentials.authentication.password = cJSON_GetObjectItemCaseSensitive(mqtt, "pwd")->valuestring;

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

