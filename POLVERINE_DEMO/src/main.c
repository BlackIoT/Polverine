#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include "bmv080.h"
#include "bmv080_io.h"


spi_device_handle_t hspi;


void bmv080_data_ready(bmv080_output_t bmv080_output, void* callback_parameters)
{
  bool is_obstructed = bmv080_output.is_static_obstructed[0] ||
                       bmv080_output.is_static_obstructed[1] ||
                       bmv080_output.is_static_obstructed[2] ||
                       bmv080_output.is_dynamic_obstructed;
  printf("{\"Runtime\":%.1f,\"PM25\":%.0f,\"obstructed\":\"%s\",\"odl\":\"%s\"}\n",
        bmv080_output.runtime_in_sec, bmv080_output.pm2_5, (is_obstructed ? "yes" : "no"), (bmv080_output.is_outside_detection_limits ? "yes" : "no"));     
}

void bmv080_task(void *pvParameter)
{
	uint16_t major = 0;
	uint16_t minor = 0;
	uint16_t patch = 0;
	char  	 git_hash[12] = {0};
	int32_t commits_ahead = 0;

  bmv080_delay(5000);
  printf("\r\nPOLVERINE demo starting\r\n");

  bmv080_status_code_t bmv080_current_status = E_BMV080_OK;
  bmv080_current_status = bmv080_get_driver_version(&major, &minor, &patch, git_hash, &commits_ahead);
  if (bmv080_current_status != E_BMV080_OK)
  {
    printf("Getting BMV080 sensor driver version failed with BMV080 status %d\r\n", bmv080_current_status);
    while (1);
  }
  printf("BMV080 sensor driver version: %d.%d.%d.%s.%ld\r\n", major, minor, patch, git_hash, commits_ahead);

  bmv080_handle_t handle = {0};

  bmv080_current_status = bmv080_open(&handle, hspi, bmv080_spi_read_16bit,  bmv080_spi_write_16bit,  bmv080_delay);
  if(bmv080_current_status != E_BMV080_OK)
  {
    printf("Initializing BMV080 failed with status %d\r\n", (int)bmv080_current_status);
    while (1);
  }

  bmv080_current_status = bmv080_reset(handle);
  if (bmv080_current_status != E_BMV080_OK)
  {
    printf("Resetting BMV080 sensor unit failed with BMV080 status %d\r\n", (int)bmv080_current_status);
    while (1);
  }

  bmv080_current_status = bmv080_start_continuous_measurement(handle);
  if(bmv080_current_status != E_BMV080_OK)
  {
    printf("Startin BMV080 failed with status %d\r\n", (int)bmv080_current_status);
    while (1);
  }

  for(;;)
  {
    bmv080_delay(1000);
	  bmv080_current_status = bmv080_serve_interrupt(handle,bmv080_data_ready,NULL);
    if(bmv080_current_status != E_BMV080_OK)
    {
      printf("Readinig BMV080 failed with status %d\r\n", (int)bmv080_current_status);
    }
  }
}


void app_main() 
{

  esp_err_t comm_status = spi_init(&hspi);
  if(comm_status != ESP_OK)
  {
    printf("Initializing the SPI communication interface failed with status %d\r\n", (int)comm_status);
    while (1);
  }

  xTaskCreate(&bmv080_task, "bmv080_task", 60 * 1024, NULL, configMAX_PRIORITIES - 1, NULL);

}
