#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include "peripherals.h"
#include "bmv080.h"
#include "bmv080_io.h"
#include "mqtt_client.h"
#include "driver/temperature_sensor.h"

#include "polverine_cfg.h"

spi_device_handle_t hspi;
//extern bool isConnected;
//extern esp_mqtt_client_handle_t client;
extern void bmv080_publish(const char *buffer);
extern char shortId[7];
temperature_sensor_handle_t temp_sensor = NULL;
volatile bool flBMV080Published = false;


void bmv080_data_ready(bmv080_output_t bmv080_output, void* callback_parameters)
{
static char buffer[256] = {0};
//  gpio_set_level(B_LED_PIN, 1);
//  gpio_hold_en(B_LED_PIN);

float temperature;
  temperature_sensor_get_celsius(temp_sensor, &temperature);

  snprintf(buffer,256,"{\"ID\":\"%s\",\"R\":%.1f,\"PM10\":%.0f,\"PM25\":%.0f,\"PM1\":%.0f,\"obst\":\"%s\",\"omr\":\"%s\",\"T\":%.1f, \"dcp\":%d}\n", shortId,
        bmv080_output.runtime_in_sec, bmv080_output.pm10_mass_concentration, bmv080_output.pm2_5_mass_concentration, bmv080_output.pm1_mass_concentration,
        (bmv080_output.is_obstructed ? "yes" : "no"), (bmv080_output.is_outside_measurement_range ? "yes" : "no"),
        temperature, PLVN_CFG_BMV080_DUTY_CYCLE_PERIOD_S);

//  printf(buffer);

  bmv080_publish(buffer);
  flBMV080Published = true;
//  gpio_set_level(B_LED_PIN, 0);
//  gpio_hold_en(B_LED_PIN);
}

uint32_t get_tick_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void bmv080_task(void *pvParameter)
{

  esp_err_t comm_status = spi_init(&hspi);
  if(comm_status != ESP_OK)
  {
    printf("Initializing the SPI communication interface failed with status %d\r\n", (int)comm_status);
    vTaskDelete(NULL);
  }

	uint16_t major = 0;
	uint16_t minor = 0;
	uint16_t patch = 0;
	char  	 git_hash[12] = {0};
	int32_t commits_ahead = 0;

  bmv080_delay(5000);
  printf("\r\nPOLVERINE demo starting on %s\r\n",shortId);

  bmv080_status_code_t bmv080_current_status = E_BMV080_OK;
  bmv080_current_status = bmv080_get_driver_version(&major, &minor, &patch, git_hash, &commits_ahead);
  if (bmv080_current_status != E_BMV080_OK)
  {
    printf("Getting BMV080 sensor driver version failed with BMV080 status %d\r\n", bmv080_current_status);
    gpio_set_level(R_LED_PIN, 1);
    vTaskDelete(NULL);
  }
  printf("BMV080 sensor driver version: %d.%d.%d.%s.%ld\r\n", major, minor, patch, git_hash, commits_ahead);
  gpio_set_level(G_LED_PIN, 1);
  bmv080_delay(50);   /* >= 50 ms so the blip is actually visible (1 ms rounded to 0 ticks) */
  gpio_set_level(G_LED_PIN, 0);

  bmv080_handle_t handle = {0};

  bmv080_current_status = bmv080_open(&handle, hspi, bmv080_spi_read_16bit,  bmv080_spi_write_16bit,  bmv080_delay);
  if(bmv080_current_status != E_BMV080_OK)
  {
    printf("Initializing BMV080 failed with status %d\r\n", (int)bmv080_current_status);
    gpio_set_level(R_LED_PIN, 1);
    vTaskDelete(NULL);
  }
  gpio_set_level(G_LED_PIN, 1);
  bmv080_delay(50);   /* >= 50 ms so the blip is actually visible (1 ms rounded to 0 ticks) */
  gpio_set_level(G_LED_PIN, 0);

  bmv080_current_status = bmv080_reset(handle);
  if (bmv080_current_status != E_BMV080_OK)
  {
    printf("Resetting BMV080 sensor unit failed with BMV080 status %d\r\n", (int)bmv080_current_status);
    gpio_set_level(R_LED_PIN, 1);
    vTaskDelete(NULL);
  }
  gpio_set_level(G_LED_PIN, 1);
  bmv080_delay(50);   /* >= 50 ms so the blip is actually visible (1 ms rounded to 0 ticks) */
  gpio_set_level(G_LED_PIN, 0);

   /* --- duty-cycling particle measurement ---------------------------- */
    /* Read back the SDK default duty-cycling period, then program ours. */
    uint16_t duty_cycling_period = 0;
    bmv080_current_status = bmv080_get_parameter(handle, "duty_cycling_period", (void*)&duty_cycling_period);

    printf("BMV080 default duty-cycling period: %d s\r\n", duty_cycling_period);

    duty_cycling_period = PLVN_CFG_BMV080_DUTY_CYCLE_PERIOD_S;
    bmv080_current_status = bmv080_set_parameter(handle, "duty_cycling_period", (void*)&duty_cycling_period);

    printf("BMV080 duty-cycling period set to %d s\r\n", duty_cycling_period);


  bmv080_current_status = bmv080_start_duty_cycling_measurement(handle,get_tick_ms,E_BMV080_DUTY_CYCLING_MODE_0);
  if(bmv080_current_status != E_BMV080_OK)
  {
    printf("Starting BMV080 failed with status %d\r\n", (int)bmv080_current_status);
    gpio_set_level(R_LED_PIN, 1);
    vTaskDelete(NULL);
  }
  gpio_set_level(G_LED_PIN, 1);
  bmv080_delay(50);   /* >= 50 ms so the blip is actually visible (1 ms rounded to 0 ticks) */
  gpio_set_level(G_LED_PIN, 0);


 temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
 temperature_sensor_install(&temp_sensor_config, &temp_sensor);
 temperature_sensor_enable(temp_sensor);


  for(;;)
  {
    bmv080_delay(1000);
	  bmv080_current_status = bmv080_serve_interrupt(handle,bmv080_data_ready,NULL);
    if(bmv080_current_status != E_BMV080_OK)
    {
      printf("Reading BMV080 failed with status %d\r\n", (int)bmv080_current_status);
      gpio_set_level(R_LED_PIN, 1);
      bmv080_delay(50);
      gpio_set_level(R_LED_PIN, 0);
    }
  }
  vTaskDelete(NULL);
}


void bmv080_app_start()
{
  xTaskCreate(&bmv080_task, "bmv080_task", 60 * 1024, NULL, configMAX_PRIORITIES - 1, NULL);
}
