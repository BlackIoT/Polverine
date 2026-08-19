## POLVERINE_DEMO: Stream sensor data over USB

This is the default demo firmware pre-installed on Polverine.

### A Complete yet Simple Demo

This firmware collects sensor measurements and transmits them using the ESP32-S3 module's native USB connection. 

The system also benefits from the latest SDK for the BMV080 sensor, which is now publicly available. This SDK enables the reading of particulate matter (PM) values for PM10, PM2.5, and PM1. The full range of sensed quantities includes:

- Temperature
- Relative Humidity
- Barometric Pressure
- Total Volatile Organic Compounds (TVOC, reported in ppb in the `VOC` JSON field)
- Particulate Matter 10 (PM10)
- Particulate Matter 2.5 (PM2.5)
- Particulate Matter 1 (PM1)

### Data Visualization Made Easy

To facilitate data analysis and visualization, a separate NodeRed ["RealTime Flows"](../nodered/client_rt_flows.json) interface has been developed. This interface collects data from USB connection and plots data in real time. 
![](../images/client_flow_realtime.png)

![](../images/client_dash_realtime.png)

### Adding Bosch SDKs to Polverine source code

The POLVERINE_DEMO communicates with the Bosch BME690 & BMV080 sensors and needs the SDKs published by Bosch Sensortec.

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
The POLVERINE_DEMO enables automatic light sleep on the ESP32-S3, and while the chip is in light sleep the native USB port does not respond to the host, so the upload tool may fail to find or reset the board. To upload the firmware using USB port you must put Polverine in boot mode: 

1. press boot button (SW1 - BOOT)
2. press reset button (SW2 - EN)
3. release reset button
4. release boot button
5. upload firmware
6. press reset button
7. release reset button


When in bootloader mode the RGB led is lit.

### Low Power Mode

The application uses ESP32 power management: `pm_init()` (in `src/main.c`, called from `app_main()`) clamps the CPU at 80 MHz and enables automatic light sleep (`CONFIG_FREERTOS_USE_TICKLESS_IDLE=y`). While the firmware is actively streaming sensor JSON the frequent task wake-ups keep the chip mostly awake, but whenever it does enter light sleep the USB-Serial-JTAG port drops off the host — hence the manual boot-mode procedure above for uploading.

To run at full power (and keep the USB port always available) comment out the call to:

	pm_init();

in `src/main.c`, in function `app_main()`.
