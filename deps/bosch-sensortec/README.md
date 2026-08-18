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
repository cannot ship them — not even patched, and not even as a unified diff
(a diff quotes the original). Instead it ships only BlackIoT's *modifications*,
as anchored search/replace edits inside
[`bsec_library_wrappers/apply_polverine_patches.py`](bsec_library_wrappers/apply_polverine_patches.py),
and you regenerate the files from your own download:

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

Run `--help` for the options; `--src` / `--dst` override the paths, and
`--drop-breath-voc` omits the compatibility edits described below.

## 4. Verify

```bash
python3 deps/bosch-sensortec/bsec_library_wrappers/apply_polverine_patches.py   # every edit reports "ok"
cd POLVERINE_DEMO && pio run                                                    # or any other demo
```

`before_build.py` runs first and checks all 14 required files. A silent pass
means staging is complete. Then confirm nothing leaked into git:

```bash
git status --short          # must not list anything under deps/bosch-sensortec/
git check-ignore -v deps/bosch-sensortec/bsec_library_wrappers/bsec_integration.c
```

## 5. What this repository *does* track under `deps/bosch-sensortec/`

| Path | Licence | Why it is tracked |
| --- | --- | --- |
| `BME690_SensorAPI/` | BSD-3-Clause (see its `LICENSE`) | Bosch publishes it as open source; redistribution is permitted |
| `bsec_library_wrappers/bsec_temp_offset_tab_target_board.h` | BlackIoT | Polverine's own self-heating compensation table |
| `bsec_library_wrappers/apply_polverine_patches.py` | BlackIoT | the modifications described in step 3 |
| `README.md` (this file) | BlackIoT | — |

Everything else here is yours, staged locally, and stays out of git.

---

## Notes on the 3.3.0.0 upgrade

- **Breath VOC.** Bosch dropped `BSEC_OUTPUT_BREATH_VOC_EQUIVALENT` from the
  3.3.0.0 example, but all four demos publish it as their `VOC` value, so
  `apply_polverine_patches.py` re-adds the subscription and the struct field by
  default. The enum value still exists in 3.3.0.0, so this compiles and
  subscribes cleanly. **Whether `libalgobsec.a` 3.3.0.0 still emits that output
  has not been confirmed on hardware.** If your `VOC` reading is a constant
  `0.00`, switch to `output->tvoc_equivalent` (also present in the generated
  header) in the demo's `bme690_main.c`, and run the script with
  `--drop-breath-voc`.
- **Stored BSEC state.** `BSEC_MAX_STATE_BLOB_SIZE` shrank from 201 to 197 and
  `BSEC_MAX_PROPERTY_BLOB_SIZE` from 554 to 550. `POLVERINE_HOMEASSISTANT_DEMO`
  persists BSEC state in NVS; a blob written by BSEC 3.2.1.0 can make
  `bsec_set_state()` return `BSEC_E_CONFIG_VERSIONMISMATCH`, which that demo
  treats as fatal for the BME690 task. **Erase NVS once when upgrading**
  (`pio run -t erase`, or `idf.py erase-flash`).
