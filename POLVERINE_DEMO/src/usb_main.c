#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/usb_serial_jtag.h"
#include <stdio.h>
#include <string.h>



#define BUFFER_SIZE 1024

void usb_task(void *)
{
    usb_serial_jtag_driver_config_t config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    usb_serial_jtag_driver_install(&config);

    uint8_t buf[BUFFER_SIZE];
    while (1) {
        // Read from USB CDC; block until data arrives instead of polling
        // every tick (a 1-tick poll loop defeats automatic light sleep)
        int len = usb_serial_jtag_read_bytes(buf, BUFFER_SIZE - 1, portMAX_DELAY);
        if (len > 0) {
            buf[len] = '\0';
            ESP_LOGI("USB", "Received %d bytes: %.*s", len, len, buf);

            // Write back to USB CDC
            usb_serial_jtag_write_bytes(buf, len, portMAX_DELAY);
        }
    }
    vTaskDelete(NULL);
}

void usb_app_start(void) 
{
  xTaskCreate(&usb_task, "usb_task", 60 * 1024, NULL, configMAX_PRIORITIES - 1, NULL);
}
