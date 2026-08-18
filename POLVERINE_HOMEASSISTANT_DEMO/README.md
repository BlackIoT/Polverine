## POLVERINE_FULL_MQTT_DEMO: A Complete Solution for Sensor Data Collection and Analysis

### A Comprehensive System Architecture

At the heart of this solution lies POLVERINE_FULL_MQTT_DEMO firmware. This firmware collects sensor measurements and transmits them using the ESP32-S3 module's WiFi capabilities to an MQTT broker. From there, a NodeRed ["Server Flows"](../nodered/server_flows.json) monitors the broker's message queue and systematically archives all incoming data in a MongoDB database.

![](../images/scada_flow.png)

The system also benefits from the latest SDK for the BMV080 sensor, which is now publicly available. This SDK enables the reading of particulate matter (PM) values for PM10, PM2.5, and PM1. The full range of sensed quantities includes:

- Temperature
- Relative Humidity
- Barometric Pressure
- Volatile Organic Compounds (V.O.C.)
- Particulate Matter 10 (PM10)
- Particulate Matter 2.5 (PM2.5)
- Particulate Matter 1 (PM1)

In addition to local sensor data, the server-side processing layer integrates with OpenWeatherMap's API to enrich the dataset with external environmental parameters such as temperature, humidity, barometric pressure, wind speed, and cloud cover percentages.

### Data Visualization Made Easy

To facilitate data analysis and visualization, a separate NodeRed ["Client Flows"]((../nodered/client_flows.json)) interface has been developed. This interface allows users to dynamically query the MongoDB repository and visualize data directly in a web browser. Users can select metrics from individual devices or compare data from multiple units, with flexible time filters enabling detailed examination of trends over configurable periods.

### Adding Bosch SDKs to Polverine source code

The POLVERINE_HOMEASSISTANT_DEMO communicates with the Bosch BME690 & BMV080 sensors and needs the SDKs published by Bosch Sensortec.

Those SDKs are licence-gated: their sources and compiled libraries cannot be stored in this repository, and must be downloaded by the user from the Bosch website by accepting the licence agreement, then staged under `../deps/bosch-sensortec/`.

- [BMV080 SDK r.11.2.0](https://www.bosch-sensortec.com/software-tools/double-opt-in-forms/sdk-v11-2.html) - unzip into `../deps/bosch-sensortec/BMV080-SDK/`
- [BME690 BSEC r.3.3.0.0](https://www.bosch-sensortec.com/software-tools/double-opt-in-forms/bsec-software-3-3-0-0-form.html) - unzip into `../deps/bosch-sensortec/bsec_library_v3.x/`

`bsec_integration.c` / `bsec_integration.h` also derive from Bosch example code and are not in this repository; regenerate them from your own BSEC download with:

```
python3 deps/bosch-sensortec/bsec_library_wrappers/apply_polverine_patches.py
```

**The complete, authoritative staging procedure - exact directory layout, the file-by-file list and how to verify it - is in [../deps/bosch-sensortec/README.md](../deps/bosch-sensortec/README.md). Follow it before building.**

`before_build.py` runs before compilation and aborts with the full path of the first missing file, so an incomplete staging can never turn into a confusing compiler error.

> BSEC **3.2.1.0 and older will not build**: the library tree moved from `algo/bsec_IAQ/` to `release_bin/IAQ/` in 3.3.0.0.

### Firmware Customization

To make the application work correctly in your environment you must customize some settings:

#### WiFi connection configuration

- in the file /src/wifi_connect.c must replace the dummy xxxxxxxxx in DEFAUT_POLVERINE_WIFI with actual values, for example:

		const char *DEFAULT_POLVERINE_WIFI = "{\"ssid\":\"AccessPointID\",\"pwd\":\"AccessPointPassword\"}";

#### MQTT connection configuration

- in the file /src/mqtt_main.c must replace the dummy xxxxxxxxx in TEMPLATE_POLVERINE_MQTT with actual values, for example:

		const char *TEMPLATE_POLVERINE_MQTT = "{\"uri\":\"mqtt://mqttserver.local\",\"user\":\"username\",\"pwd\":\"userpassword\",\"clientid\":\"%s\"}";

	- uri : MQTT broker address
	- user : username to have access to MQTT broker
	- pwd : password assigned to the user
	- clientid : you can use the %s placeholder to have a unique ID or specify your own .

- you can customize MQTT topics changing the values in TEMPLATE_POLVERINE_TOPIC .

		const char *TEMPLATE_POLVERINE_TOPIC = "{\"bmv080\":\"polverine/%s/bmv080\",\"bme690\":\"polverine/%s/bme690\",\"cmd\":\"polverine/%s/cmd\"}";

	- bmv080 : MQTT topic to publish BMV080 sensor data
	- bme690 : MQTT topic to publish BME690 sensor data
	- sys: MQTT topic to publish Polverine system data
	- cmd : MQTT topic to subscribe to commands sent to the device

the %s placeholder is dynamically substituted with the unique ID of the device, i.e. with the 6 terminating characters of the MAC address of the device.


### Firmware Upload
The POLVERINE_FULL_MQTT_DEMO puts the ESP32-S3 MCU in low power mode that disables USB. To upload the firmware using USB port you must put Polverine in boot mode: 

1. press boot button (SW1 - BOOT)
2. press reset button (SW2 - EN)
3. release reset button
4. release boot button
5. upload firmware
6. press reset button
7. release reset button


When in bootloader mode the RGB led is lit.
