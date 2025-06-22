/*!
 * @file bsec_iot_example.c
 *
 * @brief
 * Example for using of BSEC library in a fixed configuration with the BME68x sensor.
 * This works by running an endless loop in the bsec_iot_loop() function.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bsec_iaq.h"
#include "bsec_integration.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "peripherals.h"
#include "sensorbuffer.h"
#include "polverine_cfg.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_system.h"

#define SID_BME69X                      UINT16_C(0x093)
#define SID_BME69X_X8                   UINT16_C(0x057)
#define I2C_MASTER_SCL_IO           21      // GPIO number for I2C master clock
#define I2C_MASTER_SDA_IO           14      // GPIO number for I2C master data
#define I2C_MASTER_NUM              I2C_NUM_0   // I2C port number for master dev
#define I2C_MASTER_FREQ_HZ          100000  // I2C master clock frequency
#define BME690_SENSOR_ADDR          0x76    // BME690 sensor address

// NVS storage constants for BME690 state
#define BME690_NVS_NAMESPACE        "bme690"
#define BME690_NVS_STATE_KEY        "bsec_state"
#define BME690_STATE_SAVE_PERIOD_MS 3600000  // Save state every hour

static uint8_t dev_addr[NUM_OF_SENS];
uint8_t bme_cs[NUM_OF_SENS];
extern uint8_t n_sensors;
extern uint8_t *bsecInstance[NUM_OF_SENS];

static i2c_master_dev_handle_t dev_handle;
static i2c_master_bus_handle_t bus_handle;

extern void bme690_publish(const char *buffer);
extern char shortId[7];

/* BME communication methods */

static BME69X_INTF_RET_TYPE bme69x_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr);

static BME69X_INTF_RET_TYPE bme69x_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr);

static void bme69x_delay_us(uint32_t period, void *intf_ptr);

/* Callback methods */
static void bme69x_interface_init(struct bme69x_dev *bme, uint8_t intf, uint8_t sen_no);

static uint32_t state_load(uint8_t *state_buffer, uint32_t n_buffer);

static uint32_t config_load(uint8_t *config_buffer, uint32_t n_buffer);

static uint32_t get_timestamp_ms();

static void state_save(const uint8_t *state_buffer, uint32_t length);

static void output_ready(outputs_t *output);

void i2c_init(void)
{
        static i2c_master_bus_config_t i2c_mst_config_1 = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = -1,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config_1, &bus_handle));


    static i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BME690_SENSOR_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));
}

void i2c_deinit(void)
{
ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle));
}

sensorbuffer aveT;
sensorbuffer aveP;
sensorbuffer aveH;
sensorbuffer aveIAQ;
sensorbuffer aveACC;
sensorbuffer aveCO2;
sensorbuffer aveVOC;

void bme690_task(void *)
{
    i2c_init();

    sb_init(&aveT,60);
    sb_init(&aveP,60);
    sb_init(&aveH,60);
    sb_init(&aveIAQ,60);
    sb_init(&aveACC,60);
    sb_init(&aveCO2,60);
    sb_init(&aveVOC,60);

    bsec_version_t version;
    return_values_init ret = {BME69X_OK, BSEC_OK};

    ret = bsec_iot_init(SAMPLE_RATE, bme69x_interface_init, state_load, config_load);

    ESP_LOGI("BME690", "BSEC initialized with CONTINUOUS sampling rate (1Hz)");

	if (ret.bme69x_status != BME69X_OK) {
		printf("ERROR while initializing BME68x: %d\r\n", ret.bme69x_status);
        return;
	}
	if (ret.bsec_status < BSEC_OK) {
		printf("\nERROR while initializing BSEC library: %d\n", ret.bsec_status);
        return;
	}
	else if (ret.bsec_status > BSEC_OK) {
		printf("\nWARNING while initializing BSEC library: %d\n", ret.bsec_status);
	}

	ret.bsec_status = bsec_get_version(bsecInstance, &version);

    printf("BSEC Version : %u.%u.%u.%u\r\n",version.major,version.minor,version.major_bugfix,version.minor_bugfix);
/*
#if (OUTPUT_MODE == IAQ)
    static char *header = "Time(ms), IAQ,  IAQ_accuracy, Static_IAQ, Raw_Temperature(degC), Raw_Humidity(%%rH), Comp_Temperature(degC),  Comp_Humidity(%%rH), Raw_pressure(Pa), Raw_Gas(ohms), Gas_percentage, CO2, bVOC, Stabilization_status, Run_in_status, Bsec_status\r\n";
#else
    static char *header = "Time(ms), Class/Target_1_prediction, Class/Target_2_prediction, Class/Target_3_prediction, Class/Target_4_prediction, Prediction_accuracy_1, Prediction_accuracy_2, Prediction_accuracy_3, Prediction_accuracy_4, Raw_pressure(Pa), Raw_Temperature(degC),  Raw_Humidity(%%rH), Raw_Gas(ohm), Raw_Gas_Index(num), Bsec_status\r\n";
#endif

    printf(header);
*/
    bsec_iot_loop(state_save, get_timestamp_ms, output_ready);

    i2c_deinit();
}


static BME69X_INTF_RET_TYPE bme69x_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    uint8_t device_addr = *(uint8_t*)intf_ptr;

    (void)intf_ptr;

    return i2c_master_transmit_receive(dev_handle, &reg_addr, 1, reg_data, len, -1);
}

static BME69X_INTF_RET_TYPE bme69x_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
static uint8_t buffer[257];

    (void)intf_ptr;

if(len < 256)
{
    buffer[0] = reg_addr;
    memcpy(&buffer[1],reg_data,len);
    return i2c_master_transmit(dev_handle, buffer, len+1, -1);
}
else
    return -1;
}

static void bme69x_delay_us(uint32_t period, void *intf_ptr)
{
    (void)intf_ptr;
    vTaskDelay(period / (portTICK_PERIOD_MS*1000));
}

static void bme69x_interface_init(struct bme69x_dev *bme, uint8_t intf, uint8_t sen_no)
{
    if (bme == NULL)
        return;

    /* Bus configuration : I2C */
        //dev_addr[sen_no] = BME69X_I2C_ADDR_LOW;
        bme->read = bme69x_i2c_read;
        bme->write = bme69x_i2c_write;
        bme->intf = BME69X_I2C_INTF;
        bme->delay_us = bme69x_delay_us;
        bme->intf_ptr = 0; //&dev_addr[sen_no];
        bme->amb_temp = 22; /* The ambient temperature in deg C is used for defining the heater temperature */

}

static uint32_t state_load(uint8_t *state_buffer, uint32_t n_buffer)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;
    size_t length = n_buffer;
    
    ESP_LOGI("BME690", "Loading BSEC state from NVS...");
    
    // Open NVS handle
    err = nvs_open(BME690_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW("BME690", "Failed to open NVS for reading: %s", esp_err_to_name(err));
        return 0;
    }
    
    // Get the state data
    err = nvs_get_blob(nvs_handle, BME690_NVS_STATE_KEY, state_buffer, &length);
    nvs_close(nvs_handle);
    
    if (err == ESP_OK) {
        ESP_LOGI("BME690", "Successfully loaded BSEC state from NVS (%lu bytes)", (unsigned long)length);
        return length;
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI("BME690", "No saved BSEC state found in NVS");
    } else {
        ESP_LOGW("BME690", "Failed to load BSEC state: %s", esp_err_to_name(err));
    }
    
    return 0;
}

static uint32_t config_load(uint8_t *config_buffer, uint32_t n_buffer)
{
	memcpy(config_buffer, bsec_config_iaq, n_buffer);
    return n_buffer;
}

static uint32_t get_timestamp_ms()
{
     uint32_t system_current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    return system_current_time;
}

// Variables to track the latest state for forced saves
static uint8_t *latest_state_buffer = NULL;
static uint32_t latest_state_length = 0;

// Forward declaration
static void bme690_force_state_save(void);

static void state_save(const uint8_t *state_buffer, uint32_t length)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;
    static uint32_t last_save_time = 0;
    uint32_t current_time = get_timestamp_ms();
    
    // Update the latest state buffer
    if (latest_state_buffer == NULL || latest_state_length < length) {
        latest_state_buffer = realloc(latest_state_buffer, length);
    }
    if (latest_state_buffer != NULL) {
        memcpy(latest_state_buffer, state_buffer, length);
        latest_state_length = length;
    }
    
    // Only save state periodically to reduce NVS wear
    if (current_time - last_save_time < BME690_STATE_SAVE_PERIOD_MS && last_save_time != 0) {
        return;
    }
    
    ESP_LOGI("BME690", "Saving BSEC state to NVS (%lu bytes)...", (unsigned long)length);
    
    // Open NVS handle
    err = nvs_open(BME690_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE("BME690", "Failed to open NVS for writing: %s", esp_err_to_name(err));
        return;
    }
    
    // Save the state data
    err = nvs_set_blob(nvs_handle, BME690_NVS_STATE_KEY, state_buffer, length);
    if (err != ESP_OK) {
        ESP_LOGE("BME690", "Failed to save BSEC state: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return;
    }
    
    // Commit the changes
    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (err == ESP_OK) {
        ESP_LOGI("BME690", "Successfully saved BSEC state to NVS");
        last_save_time = current_time;
    } else {
        ESP_LOGE("BME690", "Failed to commit BSEC state: %s", esp_err_to_name(err));
    }
}


extern volatile bool flBMV080Published;

extern float extTempOffset;

static void output_ready(outputs_t *output)
{
static char* buffer[600];
static uint8_t last_iaq_accuracy = 0;
static uint32_t startup_time = 0;
static bool first_output = true;

  if (first_output) {
      startup_time = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
      first_output = false;
      ESP_LOGI("BME690", "First BSEC output received - starting calibration period");
  }

  //gpio_set_level(G_LED_PIN, 1);
  //gpio_hold_en(G_LED_PIN);
  //gpio_deep_sleep_hold_en();

  sb_add(&aveT,output->compensated_temperature);
  sb_add(&aveP,output->raw_pressure);
  sb_add(&aveH,output->compensated_humidity);
  sb_add(&aveIAQ,output->iaq);
  sb_add(&aveACC,output->iaq_accuracy);
  sb_add(&aveCO2,output->co2_equivalent);
  sb_add(&aveVOC,output->breath_voc_equivalent);
  
  // Log detailed status information
  uint32_t uptime = (xTaskGetTickCount() * portTICK_PERIOD_MS / 1000) - startup_time;
  
  // Log accuracy status with meaning
  const char* accuracy_desc = "Unknown";
  switch(output->iaq_accuracy) {
      case 0: accuracy_desc = "Stabilizing"; break;
      case 1: accuracy_desc = "Low"; break;
      case 2: accuracy_desc = "Medium"; break;
      case 3: accuracy_desc = "High"; break;
  }
  
  // Log stabilization and run-in status
  const char* stab_status = output->stabStatus ? "Stabilized" : "Stabilizing";
  const char* runin_status = output->runInStatus ? "Complete" : "Running";
  
  ESP_LOGI("BME690", "Uptime: %lu s, Accuracy: %d (%s), Stabilization: %s, Run-in: %s", 
           (unsigned long)uptime, output->iaq_accuracy, accuracy_desc, stab_status, runin_status);
  
  // Force save when IAQ accuracy improves
  if (output->iaq_accuracy > last_iaq_accuracy) {
      ESP_LOGI("BME690", "IAQ accuracy improved: %d -> %d", last_iaq_accuracy, output->iaq_accuracy);
      last_iaq_accuracy = output->iaq_accuracy;
      // Force a save when accuracy improves
      if (latest_state_buffer != NULL && latest_state_length > 0) {
          bme690_force_state_save();
      }
  }

{ // called 1 time every 3 ? (4.25) seconds, demultiplied to 1 data out every minute
//    static int demult = 20;

//    if(demult++ >= 20)

    if (!PVLN_CFG_BSEC_OUTPUT_UPDATE_GATED_BY_BMV080) {
        snprintf((char * __restrict__)buffer,600,"{\"ID\":\"%s\",\"R\":%.2f,\"T\":%.2f,\"P\":%.2f,\"H\":%.2f,\"IAQ\":%.2f,\"ACC\":%.2f,\"CO2\":%.2f,\"VOC\":%.2f,"
                "\"SIAQ\":%.2f,\"GASP\":%.2f,\"STAB\":%.0f,\"RUN\":%.0f,"
                "\"mtof\":%.2f, \"bougb8\":%d, \"ltdt\":%d}\n",
                shortId,
                (float)(output->timestamp/1000000)/1000., output->compensated_temperature, output->raw_pressure, output->compensated_humidity,
                (float)output->iaq, (float)output->iaq_accuracy, output->co2_equivalent, output->breath_voc_equivalent,
                output->static_iaq, output->gas_percentage, output->stabStatus, output->runInStatus,
                extTempOffset, PVLN_CFG_BSEC_OUTPUT_UPDATE_GATED_BY_BMV080, PLVN_CFG_BSEC_LOOP_DELAY_TIME_MS);

        bme690_publish((const char *)buffer);
        
        // Log the sensor values
        ESP_LOGI("BME690", "Temperature: %.2f°C, Pressure: %.2f Pa, Humidity: %.2f%%", 
                output->compensated_temperature, output->raw_pressure, output->compensated_humidity);
        ESP_LOGI("BME690", "IAQ: %.2f (acc: %d), CO2: %.2f ppm, VOC: %.2f ppm", 
                (float)output->iaq, output->iaq_accuracy, output->co2_equivalent, output->breath_voc_equivalent);
        ESP_LOGI("BME690", "Raw gas: %.2f ohms, Gas percentage: %.2f%%, Static IAQ: %.2f", 
                output->raw_gas, output->gas_percentage, output->static_iaq);
    } 
    else if(PVLN_CFG_BSEC_OUTPUT_UPDATE_GATED_BY_BMV080 && flBMV080Published)
    {
        snprintf((char * __restrict__)buffer,600,"{\"ID\":\"%s\",\"R\":%.2f,\"T\":%.2f,\"P\":%.2f,\"H\":%.2f,\"IAQ\":%.2f,\"ACC\":%.2f,\"CO2\":%.2f,\"VOC\":%.2f,"
                "\"SIAQ\":%.2f,\"GASP\":%.2f,\"STAB\":%.0f,\"RUN\":%.0f,"
                "\"mtof\":%.2f, \"bougb8\":%d, \"ltdt\":%d}\n",
                shortId,
                (float)(output->timestamp/1000000)/1000., sb_average(&aveT), sb_average(&aveP), sb_average(&aveH),
                sb_average(&aveIAQ), sb_average(&aveACC), sb_average(&aveCO2), sb_average(&aveVOC),
                output->static_iaq, output->gas_percentage, output->stabStatus, output->runInStatus,
                extTempOffset, PVLN_CFG_BSEC_OUTPUT_UPDATE_GATED_BY_BMV080, PLVN_CFG_BSEC_LOOP_DELAY_TIME_MS);

        bme690_publish((const char *)buffer);
        
        // Log the averaged sensor values
        ESP_LOGI("BME690", "Averaged - Temp: %.2f°C, Press: %.2f Pa, Hum: %.2f%%", 
                sb_average(&aveT), sb_average(&aveP), sb_average(&aveH));
        ESP_LOGI("BME690", "Averaged - IAQ: %.2f (acc: %.0f), CO2: %.2f ppm, VOC: %.2f ppm", 
                sb_average(&aveIAQ), sb_average(&aveACC), sb_average(&aveCO2), sb_average(&aveVOC));
        ESP_LOGI("BME690", "Raw gas: %.2f ohms, Gas percentage: %.2f%%, Static IAQ: %.2f", 
                output->raw_gas, output->gas_percentage, output->static_iaq);
                
        flBMV080Published = false;
        //demult = 0;
    }
}
  //gpio_set_level(G_LED_PIN, 0);
  //gpio_hold_en(G_LED_PIN);
  //gpio_deep_sleep_hold_en();

/*
#if (OUTPUT_MODE == IAQ)
    printf("%ld,%f,%u,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\r\n", (uint32_t)(output->timestamp/1000000),
                output->iaq, output->iaq_accuracy, output->static_iaq, output->raw_temp, output->raw_humidity, output->compensated_temperature,
                output->compensated_humidity, output->raw_pressure, output->raw_gas, output->gas_percentage, output->co2_equivalent,
                output->breath_voc_equivalent, output->stabStatus, output->runInStatus);
#else
    printf("%ld,%f,%f,%f,%f,%u,%u,%u,%u,%f,%f,%f,%f,%u\r\n", (uint32_t)(output->timestamp/1000000),
                output->gas_estimate_1, output->gas_estimate_2, output->gas_estimate_3, output->gas_estimate_4,
                output->gas_accuracy_1, output->gas_accuracy_2, output->gas_accuracy_3, output->gas_accuracy_4,
                output->raw_pressure, output->raw_temp, output->raw_humidity, output->raw_gas, output->raw_gas_index);
#endif
*/
}

// Force save the latest BME690 state (used during shutdown)
static void bme690_force_state_save(void)
{
    if (latest_state_buffer != NULL && latest_state_length > 0) {
        nvs_handle_t nvs_handle;
        esp_err_t err;
        
        ESP_LOGI("BME690", "Force saving BSEC state to NVS (%lu bytes)...", (unsigned long)latest_state_length);
        
        err = nvs_open(BME690_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
        if (err == ESP_OK) {
            err = nvs_set_blob(nvs_handle, BME690_NVS_STATE_KEY, latest_state_buffer, latest_state_length);
            if (err == ESP_OK) {
                err = nvs_commit(nvs_handle);
                if (err == ESP_OK) {
                    ESP_LOGI("BME690", "Successfully forced BSEC state save");
                }
            }
            nvs_close(nvs_handle);
        }
        
        if (err != ESP_OK) {
            ESP_LOGE("BME690", "Failed to force save BSEC state: %s", esp_err_to_name(err));
        }
    }
}

void bme690_app_start()
{
  xTaskCreate(&bme690_task, "bme690_task", 60 * 1024, NULL, configMAX_PRIORITIES - 1, NULL);
  
  // Register shutdown handler to save state
  ESP_ERROR_CHECK(esp_register_shutdown_handler(bme690_force_state_save));
}
