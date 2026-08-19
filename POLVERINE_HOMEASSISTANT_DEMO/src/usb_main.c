#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/usb_serial_jtag.h"
#include <stdio.h>
#include <string.h>

extern void mqtt_store_write(const char *buffer);
extern void topic_store_write(const char *buffer);

void parse(const char *buffer)
{
    printf("In Parse\r\n");
    if(!strncmp(buffer,"TOPIC=",6))
    {
    printf("In Topic\r\n");
       topic_store_write(&buffer[6]); 
    }
    else if(!strncmp(buffer,"MQTT=",5))
    {
    printf("In MQTT\r\n");
       mqtt_store_write(&buffer[5]);
    }
}


#define BUFFER_SIZE 1024

void usb_task(void *)
{
    usb_serial_jtag_driver_config_t config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    usb_serial_jtag_driver_install(&config);

    uint8_t buf[BUFFER_SIZE];
    while (1) {
        // Read from USB CDC. Blocking read: a zero-timeout poll every tick
        // kept the CPU busy and defeated auto light sleep. BUFFER_SIZE - 1
        // leaves room for the NUL terminator below.
        int len = usb_serial_jtag_read_bytes(buf, BUFFER_SIZE - 1, portMAX_DELAY);
        if (len > 0) {
            ESP_LOGI("USB", "Received %d bytes: %.*s", len, len, buf);

            // Write back to USB CDC
            usb_serial_jtag_write_bytes(buf, len, portMAX_DELAY);
            buf[len] = 0;
            parse((char *)buf);
        }
    }
}

void usb_app_start(void) 
{
  xTaskCreate(&usb_task, "usb_task", 60 * 1024, NULL, configMAX_PRIORITIES - 1, NULL);
}
