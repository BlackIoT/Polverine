## POLVERINE_HOMEASSISTANT_DEMO: Home Assistant Integration via MQTT Discovery

### What this demo does

This firmware reads the Bosch BME690 (temperature, relative humidity, barometric pressure and air-quality outputs fused by the BSEC library) and the Bosch BMV080 (PM10, PM2.5, PM1 particulate matter) and publishes everything to an MQTT broker using the [Home Assistant MQTT Discovery](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery) protocol. No manual configuration is needed on the Home Assistant side: the device announces its sensors and they appear automatically under a "Polverine XXXXXX" device.

Published entities:

- Temperature, Relative Humidity, Barometric Pressure (BME690)
- Indoor Air Quality (IAQ), Static IAQ, IAQ Accuracy, CO2 equivalent, TVOC (ppb), Gas Percentage (BSEC)
- Gas Sensor Stabilized / Run-in Complete (binary sensors)
- PM10, PM2.5, PM1, Sensor Obstructed, Measurement Out of Range (BMV080)
- WiFi Signal, Free Memory, Uptime, CPU Temperature (system metrics)

State is published to `polverine/<id>/bme690/state`, `polverine/<id>/bmv080/state` and `polverine/<id>/system/state`; availability (`online`/`offline`, with an MQTT last-will) to `polverine/<id>/availability`; discovery messages to `homeassistant/sensor/polverine_<id>/.../config`. `<id>` is the last 6 hex characters of the device MAC address.

### Configuration: WiFi provisioning portal

Unlike the other demos, this firmware does not use in-source credentials. On first boot (or whenever no WiFi SSID is stored) the device opens an access point:

1. Connect to the WiFi network `Polverine-XXXXXX` (password: `12345678`)
2. Open `http://192.168.4.1` in a browser
3. Enter the WiFi SSID/password and the MQTT broker URI, username and password
4. Save; the device restarts and connects to your network

The same portal is started as a fallback when connecting with the stored credentials fails. Holding the BOOT button (SW1) for 5 seconds clears the stored configuration and restarts provisioning.

All settings live in NVS (namespace `polverine_cfg`, keys `wifi_ssid`, `wifi_pass`, `mqtt_uri`, `mqtt_user`, `mqtt_pass`, `mqtt_client`). Compile-time defaults can be injected instead of using the portal — see `src/utils/config.c` (`DEFAULT_WIFI_SSID`, `DEFAULT_WIFI_PASS`, `DEFAULT_MQTT_URI`, `DEFAULT_MQTT_USER`, `DEFAULT_MQTT_PASS`) and [README_SETUP.md](README_SETUP.md) for the `build_flags` recipe. Leaving the MQTT username/password empty connects anonymously.

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

### Firmware Upload

When light sleep is active it can disable the USB-Serial-JTAG port. To upload the firmware over USB you may need to put Polverine in boot mode:

1. press boot button (SW1 - BOOT)
2. press reset button (SW2 - EN)
3. release reset button
4. release boot button
5. upload firmware
6. press reset button
7. release reset button

When in bootloader mode the RGB led is lit.

See [README_SETUP.md](README_SETUP.md) for the build commands, the Home Assistant entity list and troubleshooting.
