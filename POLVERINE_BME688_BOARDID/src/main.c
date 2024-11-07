#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "sdkconfig.h"

#define I2C_MASTER_SCL_IO           21      // GPIO number for I2C master clock
#define I2C_MASTER_SDA_IO           14      // GPIO number for I2C master data
#define I2C_MASTER_NUM              I2C_NUM_0   // I2C port number for master dev
#define I2C_MASTER_FREQ_HZ          100000  // I2C master clock frequency
#define I2C_MASTER_TX_BUF_DISABLE   0       // I2C master doesn't need buffer
#define I2C_MASTER_RX_BUF_DISABLE   0       // I2C master doesn't need buffer

#define BME688_SENSOR_ADDR          0x76    // BME688 sensor address
#define BME688_CHIP_ID_REG          0xD0    // BME688 chip ID register
#define BME688_CHIP_ID              0x61    // Expected chip ID for BME688

static const char *TAG = "BME688_I2C";


void app_main(void)
{

i2c_master_bus_config_t i2c_mst_config_1 = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = -1,
    .scl_io_num = I2C_MASTER_SCL_IO,
    .sda_io_num = I2C_MASTER_SDA_IO,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};

i2c_master_bus_handle_t bus_handle;
ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config_1, &bus_handle));
ESP_LOGI(TAG, "i2c_new_master_bus success");

i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = BME688_SENSOR_ADDR,
    .scl_speed_hz = I2C_MASTER_FREQ_HZ,
};

i2c_master_dev_handle_t dev_handle;
ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));

ESP_ERROR_CHECK(i2c_master_probe(bus_handle, BME688_SENSOR_ADDR, -1));
ESP_LOGI(TAG, "i2c_master_probe success");

uint8_t buf[1] = {BME688_CHIP_ID_REG};
uint8_t buffer[1] = {0};
ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handle, buf, sizeof(buf), buffer, 1, -1));
ESP_LOGI(TAG, "i2c_master_transmit_receive success");

if(*buffer == BME688_CHIP_ID)
    ESP_LOGI(TAG, "BME688 Chip ID correct");
else    
    ESP_LOGI(TAG, "BME688 Chip ID WRONG");

ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle));

}