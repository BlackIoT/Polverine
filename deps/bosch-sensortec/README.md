# Staging the Bosch Sensortec SDKs

The Polverine sensor demos talk to a **Bosch BMV080** particulate sensor and a
**Bosch BME690** gas/T/RH/P sensor. Both need SDKs that Bosch publishes under a
click-through licence.

**That material is not in this repository and must never be committed to it.**
The licence forbids redistributing Bosch's sources and binaries, so you have to
download them yourself, accept the licence, and stage them under this directory
(`deps/bosch-sensortec/`). Everything you stage here is ignored by
`.gitignore`, by shape — see the "Bosch Sensortec material" block there — so an
accidental `git add -A` cannot leak it.

The demos will not build until staging is complete: each demo's
`before_build.py` runs before compilation and aborts with the exact absolute
path of the first missing file and a hint on how to get it.

---

## 1. What to download

| SDK | Version | Download (Bosch double-opt-in form) |
| --- | --- | --- |
| BMV080 SDK | **11.2.0** | <https://www.bosch-sensortec.com/software-tools/double-opt-in-forms/sdk-v11-2.html> |
| BME690 BSEC library | **3.3.0.0** | <https://www.bosch-sensortec.com/software-tools/double-opt-in-forms/bsec-software-3-3-0-0-form.html> |

> **BSEC 3.2.1.0 and older will not work.** In 3.3.0.0 Bosch moved the library
> tree from `algo/bsec_IAQ/` to `release_bin/IAQ/`, and this repository is wired
> to the 3.3.0.0 layout and API. Do not mix files from two BSEC releases: the
> headers, `libalgobsec.a` and the config blob must all come from the same
> release, or you get either a buffer overrun or
> `BSEC_E_CONFIG_VERSIONMISMATCH` at runtime.

## 2. Where to put it

Both Bosch zips extract their contents at the **root of the archive** (there is
no wrapping folder inside), so unzip each one *into* a directory with the exact
name below. Run these from the repository root:

```bash
unzip -q -d deps/bosch-sensortec/BMV080-SDK        /path/to/bmv080-sdk-v11-2-0.zip
unzip -q -d deps/bosch-sensortec/bsec_library_v3.x /path/to/bsec_v3-3-0-0.zip
```

If your unzip tool already created a folder (typically named after the zip),
just move/rename that folder to the target name instead.

| Directory under `deps/bosch-sensortec/` | Comes from | Must directly contain |
| --- | --- | --- |
| `BMV080-SDK/` | BMV080 SDK 11.2.0 zip | `api/`, `api_examples/`, `LICENSE.md`, `README.md` |
| `bsec_library_v3.x/` | BSEC 3.3.0.0 zip | `release_bin/`, `examples/`, `integration_guide/` |

The name `bsec_library_v3.x` is deliberately version-agnostic — it is the path
`deps/bsec3_lib.ini` and the four `before_build.py` files look for. Keep it.

These are the exact files the build consumes:

| File | Used for |
| --- | --- |
| `BMV080-SDK/api/inc/bmv080.h`, `bmv080_defs.h` | BMV080 API headers |
| `BMV080-SDK/api_examples/_common/inc/bmv080_example.h` | BMV080 helper header |
| `BMV080-SDK/api/lib/xtensa_esp32s3/xtensa_esp32s3_elf_gcc/release/lib_bmv080.a` | BMV080 driver, ESP32-S3 build |
| `BMV080-SDK/api/lib/xtensa_esp32s3/xtensa_esp32s3_elf_gcc/release/lib_postProcessor.a` | BMV080 post-processor |
| `bsec_library_v3.x/release_bin/IAQ/inc/bsec_datatypes.h`, `bsec_interface.h` | BSEC API headers |
| `bsec_library_v3.x/release_bin/IAQ/bin/esp/esp32_s3/libalgobsec.a` | BSEC fusion library, ESP32-S3 build |
| `bsec_library_v3.x/release_bin/IAQ/config/bme690/bme690_iaq_33v_3s_4d/bsec_iaq.{c,h}` | BSEC config blob: 3.3 V, 3 s LP cycle, 4-day calibration |
| `bsec_library_v3.x/examples/BME690/examples/bsec_iot_example/bsec_integration.{c,h}` | pristine input for step 3 below |

Note the last row: the **BME690** example, not the BME688 one that also ships in
the zip.

## 3. Generate `bsec_integration.c` / `bsec_integration.h`

The demos use a modified version of Bosch's BSEC integration example. Those two
files carry Bosch's *"All Rights Reserved. Confidential."* header, so this
repository does not ship them, pristine or patched. Instead it ships BlackIoT's
*modifications* as anchored search/replace edits inside
[`bsec_library_wrappers/apply_polverine_patches.py`](bsec_library_wrappers/apply_polverine_patches.py),
and you regenerate the files from your own download. To be precise about what
that means: each anchor in the script is a short verbatim fragment of the Bosch
example — a `#define`, a declaration, the few lines around an edit point — kept
to the minimum needed to locate the edit and to fail loudly if Bosch changes
that region. Together they quote a few dozen identifying lines and no
algorithmic content; what is not shipped is the file itself.

```bash
python3 deps/bosch-sensortec/bsec_library_wrappers/apply_polverine_patches.py
```

With no arguments it reads
`bsec_library_v3.x/examples/BME690/examples/bsec_iot_example/` and writes
`bsec_integration.c` and `bsec_integration.h` into
`deps/bosch-sensortec/bsec_library_wrappers/`, which is where
`deps/bsec3_lib.ini` and every demo's `platformio.ini` expect them. It prints
every edit it makes. **Its output is Bosch-derived: do not commit it** (it is
already gitignored).

If any anchor no longer matches — wrong BSEC version, the BME688 example, or a
future Bosch release that touched that region — the script aborts, names the
failing edit, and writes nothing.

Run `--help` for the options; `--src` / `--dst` override the paths.
`--with-breath-voc` additionally re-adds the `BSEC_OUTPUT_BREATH_VOC_EQUIVALENT`
subscription (edits H5/H6/C2a/C2b/C4). It is off by default and is not usable
for a working build — see the notes below.

## 4. Verify

```bash
python3 deps/bosch-sensortec/bsec_library_wrappers/apply_polverine_patches.py   # every edit reports "ok" (the five breath-VOC edits report "skip")
cd POLVERINE_DEMO && pio run                                                    # or any other demo
```

`before_build.py` runs first and checks all 14 required files. A silent pass
means staging is complete. Then confirm nothing leaked into git:

```bash
git status --short          # must not list anything under deps/bosch-sensortec/
```

A build also rewrites the tracked `sdkconfig.polverine` of whichever demo you
built (PlatformIO stamps its own ESP-IDF version into it), so `git status` will
usually show that file as modified. That is expected and unrelated to staging —
`git checkout -- <demo>/sdkconfig.polverine` if you want a clean tree.

```bash
git check-ignore -v deps/bosch-sensortec/bsec_library_wrappers/bsec_integration.c
```

## 5. What this repository *does* track under `deps/bosch-sensortec/`

| Path | Licence | Why it is tracked |
| --- | --- | --- |
| `BME690_SensorAPI/` | BSD-3-Clause (see its `LICENSE`) | Bosch publishes it as open source; redistribution is permitted. Tracked at **BME69x SensorAPI v1.1.0** (2026-01-30), the copy the BSEC 3.3.0.0 zip ships under `examples/BME690/src/bme69x/` — see the version table below |
| `bsec_library_wrappers/bsec_temp_offset_tab_target_board.h` | MIT (repository licence); contributed by Bosch Sensortec | Polverine board self-heating compensation tables (see the file header) |
| `bsec_library_wrappers/apply_polverine_patches.py` | BlackIoT | the modifications described in step 3 |
| `README.md` (this file) | BlackIoT | — |

Everything else here is yours, staged locally, and stays out of git.

Versions of the tracked Bosch open-source code:

| Component | Version tracked here | Origin | Note |
| --- | --- | --- | --- |
| BME69x SensorAPI (`bme69x.c`, `bme69x.h`, `bme69x_defs.h`) | **v1.1.0** (2026-01-30) | `bsec_library_v3.x/examples/BME690/src/bme69x/` of the BSEC 3.3.0.0 zip, byte-identical | Bumped from v1.0.1 (2024-05-14). v1.1.0 sign-extends the 12-bit `par_h1`/`par_h5` humidity coefficients, clamps humidity to 0..100 % and `gas_index` to <= 9, widens `t_lin` to `int64_t`, and no longer programs the ODR (sequential-mode standby) bits in `bme69x_set_conf()` — BSEC drives the sensor in forced/parallel mode, which do not use them. Public API (prototypes and macros used by the demos) unchanged. |

---

## Notes on the 3.3.0.0 upgrade

- **Breath VOC is gone — the demos now publish TVOC.** Bosch dropped
  `BSEC_OUTPUT_BREATH_VOC_EQUIVALENT` from the 3.3.0.0 example *and from the
  algorithm*. The enum value still exists in `bsec_datatypes.h`, so re-adding
  the subscription still compiles and links — but at runtime the library
  rejects the configuration with `BSEC_E_CONFIG_FEATUREMISMATCH (-35)`, which
  aborts `bsec_iot_init()` and stops the BME690 task producing anything at all.

  Verified on Polverine Rev C hardware:

  | breath-VOC subscription | boot log | bme690 output |
  |---|---|---|
  | re-added (`--with-breath-voc`) | `BME690 ERROR while initializing BSEC library: -35` | none, ever |
  | omitted (default) | `BME690 OK` / `BSEC Version : 3.3.0.0` | samples flow |

  So the four demos now read `output->tvoc_equivalent`, 3.3.0.0's replacement,
  which is subscribed automatically in LP mode. **The published JSON field is
  still named `VOC`** so existing Node-RED flows and MongoDB collections keep
  parsing — but the quantity AND THE UNIT both changed:

  | | BSEC output | unit |
  |---|---|---|
  | before (3.2.1.0) | `BSEC_OUTPUT_BREATH_VOC_EQUIVALENT` | **ppm** |
  | now (3.3.0.0) | `BSEC_OUTPUT_TVOC_EQUIVALENT` | **ppb** |

  That is a 1000x change in magnitude in a field that kept its name. Anything
  downstream that assumed ppm — dashboards, alert thresholds, stored history —
  must be rescaled or relabelled. The Home Assistant discovery message declares
  `ppb` (`POLVERINE_HOMEASSISTANT_DEMO/src/connectivity/mqtt_main.c`), and the
  repository-level `nodered/` flows (the consumers of the FULL_MQTT_DEMO
  stream) have been updated to label VOC in ppb; the `mongodb/` schema stores
  the value unchanged, so history written before the upgrade is in ppm.

  `--with-breath-voc` re-adds the breath-VOC subscription for experimentation
  only. It does not give you a BSEC 3.2.x build: the script's other anchors do
  not match the 3.2.x example, and this repository is wired to the 3.3.0.0
  layout throughout.
- **Stored BSEC state.** `BSEC_MAX_STATE_BLOB_SIZE` shrank from 201 to 197 and
  `BSEC_MAX_PROPERTY_BLOB_SIZE` from 554 to 550. `POLVERINE_HOMEASSISTANT_DEMO`
  is the only demo that persists BSEC state in NVS (the other three have empty
  Bosch stubs). A blob written by BSEC 3.2.1.0 is **not** fatal: either
  `nvs_get_blob` rejects it on length and `state_load()` returns 0 so
  `bsec_set_state()` is never called, or it is applied and returns
  `BSEC_E_CONFIG_VERSIONMISMATCH`, whose status `bsec_iot_init()` then
  overwrites with the `update_subscription()` result — so the demo's error gate
  never sees it. Either way the old calibration is **silently discarded** and
  the sensor restarts its run-in. **Erase NVS once when upgrading**
  (`pio run -t erase`, or `idf.py erase-flash`) so this is explicit rather than
  silent. On `POLVERINE_HOMEASSISTANT_DEMO` this also clears the stored WiFi
  and MQTT provisioning (the `polverine_cfg` namespace; the BSEC calibration
  state lives in `bme690`), so the board
  comes back in provisioning mode — re-provision it afterwards.
- **64-bit timestamps.** The generated `bsec_integration.h` declares
  `typedef uint64_t (*get_timestamp_ms_fct)(void);` (edit H7) and the loop
  multiplies that value into BSEC's int64 nanosecond timestamp (edit C12). Each
  demo's `get_timestamp_ms()` must therefore return 64-bit milliseconds
  (`esp_timer_get_time() / 1000`); a `uint32_t` implementation wraps after
  49.7 days, after which the BME690 output stops, and it no longer matches the
  callback type.
