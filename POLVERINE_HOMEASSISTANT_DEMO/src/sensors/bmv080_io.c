/*
 * Copyright (c) 2026 BlackIoT Sagl
 * SPDX-License-Identifier: MIT
 *
 * SPI bridge for the Bosch BMV080 SDK 11.2.0 on ESP32-S3 (written against the
 * SDK API; contains no Bosch source).
 *
 * Wire protocol expected by the SDK: every transfer is a 16-bit header word
 * followed by payload_length 16-bit words, all MSB first, within a single
 * chip-select assertion.  We map the header onto the SPI master's address
 * phase (16 address bits, no command/dummy bits) and the payload onto the
 * data phase, and byte-swap the payload words on the way in and out.
 */

#include "bmv080_io.h"

#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*
 * Polverine wiring of the BMV080 (SPI2, routed through the GPIO matrix).
 * Every value can be overridden from the build flags (-DBMV080_SPI_xxx=...)
 * when the module is used on a different board.
 */
#ifndef BMV080_SPI_HOST
#define BMV080_SPI_HOST     SPI2_HOST
#endif
#ifndef BMV080_SPI_MISO
#define BMV080_SPI_MISO     13
#endif
#ifndef BMV080_SPI_MOSI
#define BMV080_SPI_MOSI     11
#endif
#ifndef BMV080_SPI_SCLK
#define BMV080_SPI_SCLK     12
#endif
#ifndef BMV080_SPI_CS
#define BMV080_SPI_CS       10
#endif
#ifndef BMV080_SPI_HZ
#define BMV080_SPI_HZ       1000000     /* 1 MHz, mode 0 */
#endif

#define BMV080_HEADER_BITS  16
#define BMV080_WORD_BITS    16

static inline uint16_t bswap16_u16(uint16_t w)
{
    return (uint16_t)((w << 8) | (w >> 8));
}

esp_err_t spi_init(spi_device_handle_t *spi)
{
    if (spi == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *spi = NULL;

    const spi_bus_config_t bus = {
        .mosi_io_num = BMV080_SPI_MOSI,
        .miso_io_num = BMV080_SPI_MISO,
        .sclk_io_num = BMV080_SPI_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    esp_err_t err = spi_bus_initialize(BMV080_SPI_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        return err;
    }

    /* The 16-bit header travels in the address phase of every transaction. */
    const spi_device_interface_config_t dev = {
        .command_bits = 0,
        .address_bits = BMV080_HEADER_BITS,
        .dummy_bits = 0,
        .mode = 0,
        .clock_speed_hz = BMV080_SPI_HZ,
        .spics_io_num = BMV080_SPI_CS,
        .queue_size = 1,
    };
    return spi_bus_add_device(BMV080_SPI_HOST, &dev, spi);
}

esp_err_t spi_deinit(spi_device_handle_t *spi)
{
    if (spi == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    spi_device_handle_t dev = *spi;
    *spi = NULL;
    return (dev != NULL) ? spi_bus_remove_device(dev) : ESP_OK;
}

int8_t bmv080_spi_read_16bit(bmv080_sercom_handle_t handle, uint16_t header, uint16_t *payload, uint16_t payload_length)
{
    const size_t nbits = (size_t)payload_length * BMV080_WORD_BITS;

    /* Full duplex: nothing meaningful is driven on MOSI while we clock the payload in. */
    spi_transaction_t t = {
        .addr = header,
        .length = nbits,
        .rxlength = nbits,
        .rx_buffer = payload,
    };
    esp_err_t err = spi_device_polling_transmit((spi_device_handle_t)handle, &t);

    /* Words arrived MSB first; put them into host byte order. */
    for (uint16_t i = 0; i < payload_length; i++) {
        payload[i] = bswap16_u16(payload[i]);
    }

    return (int8_t)err;
}

int8_t bmv080_spi_write_16bit(bmv080_sercom_handle_t handle, uint16_t header, const uint16_t *payload, uint16_t payload_length)
{
    /*
     * The caller's buffer is const and in host byte order; build a swapped
     * copy on the heap (payload_length is under SDK control and may be large).
     */
    /* payload_length may be 0 for header-only commands: send just the
     * address phase then (tx_buffer NULL, length 0). */
    uint16_t *wire = NULL;
    if (payload_length > 0) {
        wire = malloc((size_t)payload_length * sizeof(uint16_t));
        if (wire == NULL) {
            return (int8_t)ESP_ERR_NO_MEM;
        }
        for (uint16_t i = 0; i < payload_length; i++) {
            wire[i] = bswap16_u16(payload[i]);
        }
    }

    spi_transaction_t t = {
        .addr = header,
        .length = (size_t)payload_length * BMV080_WORD_BITS,
        .tx_buffer = wire,
    };
    esp_err_t err = spi_device_transmit((spi_device_handle_t)handle, &t);

    free(wire);
    return (int8_t)err;
}

int8_t bmv080_delay(uint32_t period)
{
    vTaskDelay(pdMS_TO_TICKS(period));
    return 0;
}
