/*
 * Copyright (c) 2026 BlackIoT Sagl
 * SPDX-License-Identifier: MIT
 *
 * BME690 task for POLVERINE_DEMO: runs the Bosch BSEC 3.3.0.0 IAQ library
 * through the locally generated bsec_integration.{c,h} wrapper (see
 * deps/bosch-sensortec/README.md), averages the fused outputs and prints
 * them as JSON on the USB serial console.
 *
 * Written against the BSEC 3.3.0.0 integration example API
 * (bsec_integration.h); no Bosch source retained.
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
#include "esp_timer.h"
#include "peripherals.h"
#include "sensorbuffer.h"
#include "leds.h"
#include "polverine_cfg.h"

#define I2C_MASTER_SCL_IO           21      // GPIO number for I2C master clock
#define I2C_MASTER_SDA_IO           14      // GPIO number for I2C master data
#define I2C_MASTER_NUM              I2C_NUM_0   // I2C port number for master dev
#define I2C_MASTER_FREQ_HZ          100000  // I2C master clock frequency
#define BME690_SENSOR_ADDR          0x76    // BME690 sensor address

extern uint8_t *bsecInstance[NUM_OF_SENS];   /* BSEC instance memory, owned by bsec_integration.c */

static i2c_master_dev_handle_t dev_handle;
static i2c_master_bus_handle_t bus_handle;

extern void bme690_publish(const char *buffer);
extern char shortId[7];

/* bme69x_dev bus hooks (BME690 SensorAPI callback types), installed by bme69x_interface_init() */
static BME69X_INTF_RET_TYPE bme69x_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr);
static BME69X_INTF_RET_TYPE bme69x_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr);
static void bme69x_delay_us(uint32_t period, void *intf_ptr);

/* Hooks passed to bsec_iot_init() and bsec_iot_loop(), in argument order (see the *_fct typedefs in bsec_integration.h) */
static void bme69x_interface_init(struct bme69x_dev *bme, uint8_t intf, uint8_t sen_no);
static uint32_t state_load(uint8_t *state_buffer, uint32_t n_buffer);
static uint32_t config_load(uint8_t *config_buffer, uint32_t n_buffer);
static void state_save(const uint8_t *state_buffer, uint32_t length);
static uint64_t get_timestamp_ms(void);
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

sensorbuffer aveR;      /* raw MOX gas resistance (issue #30) */
sensorbuffer aveRawT;   /* raw die temperature */
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

    sb_init(&aveR,60);
    sb_init(&aveRawT,60);
    sb_init(&aveT,60);
    sb_init(&aveP,60);
    sb_init(&aveH,60);
    sb_init(&aveIAQ,60);
    sb_init(&aveACC,60);
    sb_init(&aveCO2,60);
    sb_init(&aveVOC,60);

    /* Probe the BME690, set up the BSEC instance (config blob, saved state, LP subscription). */
    return_values_init ret = bsec_iot_init(SAMPLE_RATE, bme69x_interface_init, state_load, config_load);
    if (ret.bme69x_status != BME69X_OK) {
        printf("POLVERINE BME690 ERROR while initializing the BME690: %d\r\n", ret.bme69x_status);
        stsBME690 = STATUS_ERROR;
        vTaskDelete(NULL);
    }
    /* BSEC return codes: negative = error, positive = warning, 0 = OK */
    if (ret.bsec_status < BSEC_OK) {
        printf("POLVERINE BME690 ERROR while initializing BSEC library: %d\r\n", ret.bsec_status);
        stsBME690 = STATUS_ERROR;
        vTaskDelete(NULL);
    }
    if (ret.bsec_status > BSEC_OK) {
        printf("POLVERINE BME690 WARNING while initializing BSEC library: %d\r\n", ret.bsec_status);
    }

    bsec_version_t version = {0};
    bsec_get_version(bsecInstance[0], &version);   /* NUM_OF_SENS == 1 */

    printf("POLVERINE BME690 OK\r\n");
    printf("BME690 BSEC library %u.%u.%u.%u\r\n", version.major, version.minor, version.major_bugfix, version.minor_bugfix);
    stsBME690 = STATUS_OK;

    bsec_iot_loop(state_save, get_timestamp_ms, output_ready);

    i2c_deinit();
    vTaskDelete(NULL);
}


static BME69X_INTF_RET_TYPE bme69x_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    (void)intf_ptr;   /* single device: addressed through dev_handle */

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
    /* Polverine has a single BME690 on I2C, so intf/sen_no add nothing here. */
    (void)intf;
    (void)sen_no;
    if (bme == NULL) {
        return;
    }
    bme->intf = BME69X_I2C_INTF;
    bme->read = bme69x_i2c_read;
    bme->write = bme69x_i2c_write;
    bme->delay_us = bme69x_delay_us;
    bme->intf_ptr = NULL;   /* the I2C hooks reach the device through dev_handle */
    bme->amb_temp = 22;     /* ambient estimate in deg C, feeds the heater set-point */
}

static uint32_t state_load(uint8_t *state_buffer, uint32_t n_buffer)
{
    /* This demo keeps no BSEC state across reboots: report "nothing stored". */
    (void)state_buffer;
    (void)n_buffer;
    return 0;
}

static uint32_t config_load(uint8_t *config_buffer, uint32_t n_buffer)
{
    /* IAQ configuration blob (bme690_iaq_33v_3s_4d), compiled in from bsec_iaq.c. */
    uint32_t n = (n_buffer < sizeof(bsec_config_iaq)) ? n_buffer : (uint32_t)sizeof(bsec_config_iaq);
    memcpy(config_buffer, bsec_config_iaq, n);
    return n;
}

static uint64_t get_timestamp_ms(void)
{
    /* 64-bit microsecond timer scaled to ms: never wraps in practice
     * (the 32-bit tick-based version wrapped after 49.7 days and froze BSEC). */
    return (uint64_t)esp_timer_get_time() / 1000ULL;
}

static void state_save(const uint8_t *state_buffer, uint32_t length)
{
    /* No persistent storage in this demo (POLVERINE_HOMEASSISTANT_DEMO shows an NVS-backed version). */
    (void)state_buffer;
    (void)length;
}


extern volatile bool flBMV080Published;

extern float extTempOffset;

static void output_ready(outputs_t *output)
{
static char buffer[600];
  
  sb_add(&aveR,output->raw_gas);
  sb_add(&aveRawT,output->raw_temp);
  sb_add(&aveT,output->compensated_temperature);
  sb_add(&aveP,output->raw_pressure);
  sb_add(&aveH,output->compensated_humidity);
  sb_add(&aveIAQ,output->iaq);
  sb_add(&aveACC,output->iaq_accuracy);
  sb_add(&aveCO2,output->co2_equivalent);
  sb_add(&aveVOC,output->tvoc_equivalent);

    if (!PVLN_CFG_BSEC_OUTPUT_UPDATE_GATED_BY_BMV080) {
        snprintf(buffer, sizeof(buffer),"{\"topic\":\"bme690\",\"data\":{\"ID\":\"%s\",\"TS\":%.2f,\"R\":%.2f,\"rawT\":%.2f,\"T\":%.2f,\"P\":%.2f,\"H\":%.2f,\"IAQ\":%.2f,\"ACC\":%.2f,\"CO2\":%.2f,\"VOC\":%.2f,"
                "\"mtof\":%.2f, \"bougb8\":%d, \"ltdt\":%d}}\n",
                shortId,
                (float)(output->timestamp/1000000)/1000., output->raw_gas, output->raw_temp, output->compensated_temperature, output->raw_pressure, output->compensated_humidity,
                (float)output->iaq, (float)output->iaq_accuracy, output->co2_equivalent, output->tvoc_equivalent,
                extTempOffset, PVLN_CFG_BSEC_OUTPUT_UPDATE_GATED_BY_BMV080, PLVN_CFG_BSEC_LOOP_DELAY_TIME_MS);

        printf("%s", buffer);
    } 
    else   /* PVLN_CFG_BSEC_OUTPUT_UPDATE_GATED_BY_BMV080 */
    {
        /* Fallback: if the BMV080 has not published for 2x its duty-cycle period
         * (failed init, dead sensor, ...) emit the BME690 data anyway instead of
         * staying silent forever. */
        static uint64_t last_emit_ms = 0;
        uint64_t now_ms = get_timestamp_ms();
        if (last_emit_ms == 0) {
            last_emit_ms = now_ms;
        }
        bool bmv080_timed_out = (now_ms - last_emit_ms) >= (uint64_t)(2 * PLVN_CFG_BMV080_DUTY_CYCLE_PERIOD_S) * 1000ULL;
        if (flBMV080Published || bmv080_timed_out)
        {
        snprintf(buffer, sizeof(buffer),"{\"topic\":\"bme690\",\"data\":{\"ID\":\"%s\",\"TS\":%.2f,\"R\":%.2f,\"rawT\":%.2f,\"T\":%.2f,\"P\":%.2f,\"H\":%.2f,\"IAQ\":%.2f,\"ACC\":%.2f,\"CO2\":%.2f,\"VOC\":%.2f,"
                "\"mtof\":%.2f, \"bougb8\":%d, \"ltdt\":%d}}\n",
                shortId,
                (float)(output->timestamp/1000000)/1000., sb_average(&aveR), sb_average(&aveRawT), sb_average(&aveT), sb_average(&aveP), sb_average(&aveH),
                sb_average(&aveIAQ), sb_average(&aveACC), sb_average(&aveCO2), sb_average(&aveVOC),
                extTempOffset, PVLN_CFG_BSEC_OUTPUT_UPDATE_GATED_BY_BMV080, PLVN_CFG_BSEC_LOOP_DELAY_TIME_MS);

        printf("%s", buffer);
        flBMV080Published = false;
        last_emit_ms = now_ms;
        }
    }
}

void bme690_app_start()
{
  xTaskCreate(&bme690_task, "bme690_task", 60 * 1024, NULL, configMAX_PRIORITIES - 1, NULL);
}
