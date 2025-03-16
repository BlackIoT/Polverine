
/**
 * @file bmv080_app.c
 * @brief BMV080 sensor application
 *
 * This file contains the implementation of the BMV080 sensor application,
 * including initialization, data reading, and MQTT publishing.
 */
 
 
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include "peripherals.h"
#include "bmv080.h"
#include "bmv080_io.h"
#include "mqtt_client.h"
#include "driver/temperature_sensor.h"
#include "ble_main.h"

/** @brief SPI device handle */
spi_device_handle_t hspi;
//extern bool isConnected;
//extern esp_mqtt_client_handle_t client;
extern void bmv080_publish(const char *buffer);
extern char shortId[7];
/** @brief Temperature sensor handle */
temperature_sensor_handle_t temp_sensor = NULL;
/** @brief Flag indicating if BMV080 data has been published */
volatile bool flBMV080Published = false;

/**
 * @brief Callback function for BMV080 data ready event
 * @param bmv080_output BMV080 sensor output data
 * @param callback_parameters Callback parameters (unused)
 */
void bmv080_data_ready(bmv080_output_t bmv080_output, void* callback_parameters)
{
static char buffer[256] = {0};
//  gpio_set_level(B_LED_PIN, 1);
//  gpio_hold_en(B_LED_PIN);

float temperature;
  temperature_sensor_get_celsius(temp_sensor, &temperature);

  snprintf(buffer,256,"{\"topic\":\"bmv080\",\"data\":{\"ID\":\"%s\",\"R\":%.1f,\"PM10\":%.0f,\"PM25\":%.0f,\"PM1\":%.0f,\"obst\":\"%s\",\"omr\":\"%s\",\"T\":%.1f}}\n", shortId,
        bmv080_output.runtime_in_sec, bmv080_output.pm10_mass_concentration, bmv080_output.pm2_5_mass_concentration, bmv080_output.pm1_mass_concentration, 
        (bmv080_output.is_obstructed ? "yes" : "no"), (bmv080_output.is_outside_measurement_range ? "yes" : "no"),
        temperature);     

//  printf(buffer);

  ble_send_message(buffer);
  flBMV080Published = true;
//  gpio_set_level(B_LED_PIN, 0);
//  gpio_hold_en(B_LED_PIN);
}

/**
 * @brief Get current system time in milliseconds
 * @return Milliseconds since system boot
 * @note Uses FreeRTOS tick count converted to milliseconds
 */
uint32_t get_tick_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

/**
 * @brief Main task function for BMV080 sensor management
 * @param pvParameter FreeRTOS task parameters (unused)
 * @details Implements the complete sensor initialization and measurement sequence:
 * 1. SPI interface initialization
 * 2. Sensor version validation
 * 3. Sensor configuration (reset, duty cycling parameters)
 * 4. Continuous measurement loop with error handling
 * 5. Temperature sensor integration
 * @see bmv080_open()
 * @see bmv080_start_duty_cycling_measurement()
 */
void bmv080_task(void *pvParameter)
{

  esp_err_t comm_status = spi_init(&hspi);
  if(comm_status != ESP_OK)
  {
    printf("Initializing the SPI communication interface failed with status %d\r\n", (int)comm_status);
    while (1);
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
    gpio_hold_en(R_LED_PIN);
    while (1);
  }
  printf("BMV080 sensor driver version: %d.%d.%d.%s.%ld\r\n", major, minor, patch, git_hash, commits_ahead);
  gpio_set_level(G_LED_PIN, 1);
  gpio_hold_en(G_LED_PIN);
  bmv080_delay(1);
  gpio_set_level(G_LED_PIN, 0);
  gpio_hold_en(G_LED_PIN);

  bmv080_handle_t handle = {0};

  bmv080_current_status = bmv080_open(&handle, hspi, bmv080_spi_read_16bit,  bmv080_spi_write_16bit,  bmv080_delay);
  if(bmv080_current_status != E_BMV080_OK)
  {
    printf("Initializing BMV080 failed with status %d\r\n", (int)bmv080_current_status);
    gpio_set_level(R_LED_PIN, 1);
    gpio_hold_en(R_LED_PIN);
    while (1);
  }
  gpio_set_level(G_LED_PIN, 1);
  gpio_hold_en(G_LED_PIN);
  bmv080_delay(1);
  gpio_set_level(G_LED_PIN, 0);
  gpio_hold_en(G_LED_PIN);

  bmv080_current_status = bmv080_reset(handle);
  if (bmv080_current_status != E_BMV080_OK)
  {
    printf("Resetting BMV080 sensor unit failed with BMV080 status %d\r\n", (int)bmv080_current_status);
    gpio_set_level(R_LED_PIN, 1);
    gpio_hold_en(R_LED_PIN);
    while (1);
  }
  gpio_set_level(G_LED_PIN, 1);
  gpio_hold_en(G_LED_PIN);
  bmv080_delay(1);
  gpio_set_level(G_LED_PIN, 0);
  gpio_hold_en(G_LED_PIN);

   /*********************************************************************************************************************
    * Running a particle measurement in duty cycling mode
    *********************************************************************************************************************/
    /* Get default parameter "duty_cycling_period" */
    uint16_t duty_cycling_period = 0;
    bmv080_current_status = bmv080_get_parameter(handle, "duty_cycling_period", (void*)&duty_cycling_period);

    printf("Default duty_cycling_period: %d s\r\n", duty_cycling_period);

    /* Set custom parameter "duty_cycling_period" */
    duty_cycling_period = 60;
    bmv080_current_status = bmv080_set_parameter(handle, "duty_cycling_period", (void*)&duty_cycling_period);

    printf("Customized duty_cycling_period: %d s\r\n", duty_cycling_period);


  bmv080_current_status = bmv080_start_duty_cycling_measurement(handle,get_tick_ms,E_BMV080_DUTY_CYCLING_MODE_0);
  if(bmv080_current_status != E_BMV080_OK)
  {
    printf("Starting BMV080 failed with status %d\r\n", (int)bmv080_current_status);
    gpio_set_level(R_LED_PIN, 1);
    gpio_hold_en(R_LED_PIN);
    while (1);
  }
  gpio_set_level(G_LED_PIN, 1);
  gpio_hold_en(G_LED_PIN);
  bmv080_delay(1);
  gpio_set_level(G_LED_PIN, 0);
  gpio_hold_en(G_LED_PIN);


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
      gpio_set_level(R_LED_PIN, 0);
    }
  }
}

/**
 * @brief Starts BMV080 monitoring task
 * @details Creates FreeRTOS task with dedicated stack space for sensor operations
 * @note Task priority set to configMAX_PRIORITIES - 1 for high priority handling
 */
void bmv080_app_start() 
{
  xTaskCreate(&bmv080_task, "bmv080_task", 60 * 1024, NULL, configMAX_PRIORITIES - 1, NULL);
}
