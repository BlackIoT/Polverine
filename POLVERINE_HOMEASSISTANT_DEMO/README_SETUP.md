# Polverine Home Assistant Sensor Setup Guide

## Overview
This firmware collects environmental data from BME690 and BMV080 sensors and reports to Home Assistant via MQTT.

## Quick Start - WiFi Provisioning

### First Time Setup
1. Power on the device
2. Device will create a WiFi access point: `Polverine-XXXXXX` (password: `12345678`)
3. Connect to this WiFi network from your phone or computer
4. Open a web browser and go to: `http://192.168.4.1`
5. Enter your WiFi and MQTT settings
6. Click "Save Configuration"
7. Device will restart and connect to your network

### Reset Configuration
- Hold the BOOT button for 5 seconds to clear all settings
- Device will restart in provisioning mode

## Alternative Configuration (Optional)

### Using Build-time flags

Add to `platformio.ini`:
```ini
build_flags = 
    ${env.build_flags}
    -DDEFAULT_WIFI_SSID=\"your-wifi-ssid\"
    -DDEFAULT_WIFI_PASS=\"your-wifi-password\"
    -DDEFAULT_MQTT_URI=\"mqtt://your-mqtt-broker.local\"
    -DDEFAULT_MQTT_USER=\"your-mqtt-username\"
    -DDEFAULT_MQTT_PASS=\"your-mqtt-password\"
```

## Manual Build Setup

1. Install dependencies:
   - Download BMV080 SDK from Bosch
   - Download BSEC3 library from Bosch
   - Stage them under `../deps/bosch-sensortec/` as specified in
     [../deps/bosch-sensortec/README.md](../deps/bosch-sensortec/README.md)

2. Build and upload:
   ```bash
   pio run -e polverine -t upload
   ```

3. Monitor output:
   ```bash
   pio device monitor
   ```

## Home Assistant Integration

The device automatically publishes discovery messages to Home Assistant. Entities will appear under:
- Device: "Polverine XXXXXX" (where XXXXXX is the device ID)
- Sensors:
  - Temperature, Humidity, Pressure (BME690)
  - Indoor Air Quality, CO2, VOC (BME690)
  - PM10, PM2.5, PM1 (BMV080)

## Troubleshooting

### Device not connecting to WiFi
- Re-enter the credentials via the provisioning portal (hold BOOT for 5 seconds to reset)
- Verify WiFi network is 2.4GHz
- Check serial output for connection errors

### MQTT connection fails
- Verify MQTT broker is running
- Check MQTT credentials
- Ensure device and broker are on same network
- Try using IP address instead of hostname

### Sensors not reporting
- BME690 requires ~5 minutes to stabilize IAQ readings
- BMV080 may show "obstructed" if airflow is blocked
- Check sensor connections on I2C bus

## Security Notes

- The device stores credentials in NVS. NVS is **not** encrypted by default;
  enable flash/NVS encryption in sdkconfig if that matters for your deployment
- Use strong passwords for WiFi and MQTT
- Consider using MQTT with TLS (mqtts://) for production
- Hold BOOT button for 5 seconds to reset credentials if needed