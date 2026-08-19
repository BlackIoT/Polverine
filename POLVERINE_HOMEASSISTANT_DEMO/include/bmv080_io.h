/*
 * Copyright (c) 2026 BlackIoT Sagl
 * SPDX-License-Identifier: MIT
 *
 * SPI bridge for the Bosch BMV080 SDK 11.2.0 on ESP32-S3 (written against the
 * SDK API; contains no Bosch source).
 *
 * The BMV080 SDK talks to the sensor through three host-supplied callbacks
 * (bmv080_callback_read_t, bmv080_callback_write_t, bmv080_callback_delay_t,
 * see bmv080_defs.h).  This module provides them on top of the ESP-IDF
 * spi_master driver, plus the bus bring-up/tear-down the application task
 * calls before/after bmv080_open().
 */
#ifndef BMV080_IO_H_
#define BMV080_IO_H_

#include <stdint.h>
#include "esp_err.h"
#include "driver/spi_master.h"
#include "bmv080.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bring up the SPI host the BMV080 hangs off and attach the sensor as a
 * device.  On success *spi holds the device handle to hand to bmv080_open()
 * as its sercom handle.  Bus and device errors are both propagated.
 */
esp_err_t spi_init(spi_device_handle_t *spi);

/*
 * Detach the sensor device created by spi_init() and clear the handle;
 * returns the driver result (ESP_OK for an already-cleared handle).
 * The SPI host itself stays initialised.
 */
esp_err_t spi_deinit(spi_device_handle_t *spi);

/*
 * SDK read/write callbacks.  header is the 16-bit register/command word,
 * payload an array of payload_length 16-bit words; on the wire everything is
 * MSB first, so the payload words are byte-swapped between the sensor's
 * big-endian framing and the ESP32's little-endian memory.  Return 0 on
 * success, otherwise the driver error truncated to int8_t.
 */
int8_t bmv080_spi_read_16bit(bmv080_sercom_handle_t handle, uint16_t header, uint16_t *payload, uint16_t payload_length);
int8_t bmv080_spi_write_16bit(bmv080_sercom_handle_t handle, uint16_t header, const uint16_t *payload, uint16_t payload_length);

/* SDK delay callback: block the calling task for about period ms
 * (FreeRTOS tick granularity — may undershoot by up to one tick). */
int8_t bmv080_delay(uint32_t period);

#ifdef __cplusplus
}
#endif

#endif /* BMV080_IO_H_ */
