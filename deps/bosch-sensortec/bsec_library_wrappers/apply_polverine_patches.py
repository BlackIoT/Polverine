#!/usr/bin/env python3
"""
apply_polverine_patches.py -- generate bsec_integration.{c,h} for Polverine.

WHY THIS SCRIPT EXISTS
----------------------
The Polverine demos need a modified copy of Bosch's BSEC integration example
(bsec_integration.c / bsec_integration.h).  Those files carry Bosch Sensortec's
"All Rights Reserved. Confidential." header, and the licence forbids delivering
the source to third parties, so this repository cannot ship them -- not even as
a patched copy, and not even as a unified diff (a diff quotes context lines of
the original).

Instead, this repository ships only BlackIoT's *modifications*, expressed as a
list of anchored search/replace edits, and this script re-applies them to the
pristine files from YOUR OWN, licence-accepted Bosch BSEC download.

INPUT
-----
BSEC v3.3.0.0 for BME690, staged as described in
deps/bosch-sensortec/README.md.  The pristine files are:

    <bsec_library_v3.x>/examples/BME690/examples/bsec_iot_example/
        bsec_integration.c
        bsec_integration.h

NOTE the zip also ships a BME688 variant under examples/BME688/ -- that one is
for a different sensor and will be rejected (its anchors do not match).

OUTPUT
------
bsec_integration.c and bsec_integration.h, written next to this script, i.e.
into deps/bosch-sensortec/bsec_library_wrappers/, which is where
deps/bsec3_lib.ini and every demo's platformio.ini expect them.  Both are
gitignored: DO NOT COMMIT THEM.

USAGE
-----
    # from anywhere; defaults assume the standard staging layout
    python3 deps/bosch-sensortec/bsec_library_wrappers/apply_polverine_patches.py

    # explicit paths
    python3 apply_polverine_patches.py --src /path/to/bsec_iot_example --dst .

    --with-breath-voc   re-add BSEC_OUTPUT_BREATH_VOC_EQUIVALENT (edits
                        H5/H6/C2a/C2b/C4).  BSEC 3.2.x ONLY -- see below.

ABOUT --with-breath-voc  (do not use with BSEC 3.3.0.0)
-------------------------------------------------------
Bosch removed BSEC_OUTPUT_BREATH_VOC_EQUIVALENT from the v3.3.0.0 example
(struct field, subscription, demux case) and from the algorithm itself.  The
enum value still exists in v3.3.0.0's bsec_datatypes.h, so re-adding the
subscription still COMPILES AND LINKS -- but it does not work: the library
rejects the configuration at init with

    BSEC_E_CONFIG_FEATUREMISMATCH (-35)
    "Enabled features encoded in serialized settings ... does not match with
     current library implementation or subscribed outputs"

which aborts bsec_iot_init(), so the BME690 task never produces a single
sample.  Verified on Polverine hardware (Rev C, MAC F0F5BD55333C): with the
group applied the log shows "BME690 ERROR while initializing BSEC library: -35"
and no bme690 output is ever emitted; without it, "BME690 OK / BSEC Version :
3.3.0.0" and samples flow.

The group is therefore OFF by default and the demos use output->tvoc_equivalent,
which is v3.3.0.0's replacement (subscribed automatically in LP mode).

LINE ENDINGS
------------
Bosch ships CRLF.  Editing is done on an LF-normalised copy and CRLF is
restored on write, so the output keeps Bosch's convention.
"""

import argparse
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# <repo>/deps/bosch-sensortec/bsec_library_v3.x/examples/BME690/examples/bsec_iot_example
DEFAULT_SRC = os.path.join(
    SCRIPT_DIR, os.pardir, "bsec_library_v3.x",
    "examples", "BME690", "examples", "bsec_iot_example",
)
DEFAULT_DST = SCRIPT_DIR


class PatchError(RuntimeError):
    pass


class Patch:
    """One anchored edit.  The anchor must occur exactly once."""

    def __init__(self, pid, summary, anchor, replacement, group=None):
        self.pid = pid
        self.summary = summary
        self.anchor = anchor
        self.replacement = replacement
        self.group = group

    def apply(self, text):
        n = text.count(self.anchor)
        if n != 1:
            raise PatchError(
                "patch %s FAILED (%s)\n"
                "  its anchor occurs %d time(s) in the input, expected exactly 1.\n"
                "  The input is probably not the BSEC v3.3.0.0 BME690 example\n"
                "  (the BME688 example and BSEC 3.2.1.0 and older will fail here),\n"
                "  or Bosch changed this region in a newer release.\n"
                "  Anchor:\n%s"
                % (self.pid, self.summary, n,
                   "\n".join("    | " + l for l in self.anchor.split("\n")))
            )
        return text.replace(self.anchor, self.replacement)


T = "\t"  # the Bosch sources use hard tabs

# ---------------------------------------------------------------------------
# bsec_integration.h
# ---------------------------------------------------------------------------

H_PATCHES = [
    Patch(
        "H1",
        "NUM_OF_SENS 8 -> 1 (Polverine carries a single BME690)",
        "#define NUM_OF_SENS    " + T + "                UINT8_C(8)",
        "#define NUM_OF_SENS    " + T + "                1 //UINT8_C(8)",
    ),
    Patch(
        "H2",
        "IAQ sample rate ULP -> LP (3 s cycle, matches the bme690_iaq_33v_3s_4d config)",
        "#define SAMPLE_RATE" + T + T + "    BSEC_SAMPLE_RATE_ULP",
        "#define SAMPLE_RATE" + T + T + "    BSEC_SAMPLE_RATE_LP",
    ),
    Patch(
        "H3",
        "Polverine self-heating offsets (Bosch appboard values kept, commented out)",
        "#define TEMP_OFFSET_LP" + T + T + "(0.1495f)\n"
        "#define TEMP_OFFSET_ULP" + T + T + "(0.466f)",
        "//#define TEMP_OFFSET_LP" + T + T + "(0.1495f)\n"
        "//#define TEMP_OFFSET_ULP" + T + T + "(0.466f)\n"
        "#define TEMP_OFFSET_LP" + T + T + "(6.1495f)\n"
        "#define TEMP_OFFSET_ULP" + T + T + "(6.466f)",
    ),
    Patch(
        "H4",
        "output_ready() takes the outputs_t struct, not a preformatted CSV string",
        "typedef void (*output_ready_fct)(char *output);",
        "typedef void (*output_ready_fct)(outputs_t *output);",
    ),
    Patch(
        "H5",
        "compat: restore outputs_t.breath_voc_equivalent (all four demos read it)",
        T + "float co2_equivalent;\n" + T + "float gas_percentage;",
        T + "float co2_equivalent;\n"
        + T + "float breath_voc_equivalent;\n"
        + T + "float gas_percentage;",
        group="breath_voc",
    ),
    Patch(
        "H6",
        "compat: NUM_USED_OUTPUTS 13 -> 14, to make room for BREATH_VOC_EQUIVALENT",
        "#define NUM_USED_OUTPUTS    UINT8_C(13)",
        "#define NUM_USED_OUTPUTS    UINT8_C(14)",
        group="breath_voc",
    ),
]

# ---------------------------------------------------------------------------
# bsec_integration.c
# ---------------------------------------------------------------------------

# BlackIoT's own code: maps the measured temperature to a board-specific
# self-heating offset via the table in bsec_temp_offset_tab_target_board.h.
C_TEMP_OFFSET_HELPER = "\n".join([
    '#include "bsec_temp_offset_tab_target_board.h"',
    "static float _get_board_specific_temp_offset(float temperature_raw) ",
    "{",
    "    uint8_t i;",
    "    uint8_t len;",
    "    const temp_offset_cf_param_t *tocp_cfg = NULL;",
    "    const temp_offset_cf_param_t *tmp;",
    "    ",
    "    if ((temperature_raw < -45.0f) || (temperature_raw > 85)) {",
    "        return 0;",
    "    }",
    "    ",
    "    const temp_profile_tab_entry_t *te;",
    "    ",
    "    ",
    "    for (i = 0; i < sizeof(temp_profile_tab)/sizeof(temp_profile_tab[0]); i++) {",
    "        te = temp_profile_tab + i;",
    "        if (PLVN_CFG_TEMP_PROFILE_CLIENT_ID == te->client_id) {",
    "            if (PLVN_CFG_BMV080_DUTY_CYCLE_PERIOD_S == te->map.bmv080_duty_cycle_period_s) {",
    "                tocp_cfg = te->map.tocp_cfg;",
    "                len = te->map.tocp_len;",
    "                break;",
    "            }",
    "        }",
    "    }",
    "",
    "    ",
    "    if (tocp_cfg) {",
    "        for (i = 0; i < len; i++) {",
    "            tmp = tocp_cfg + i;",
    "            float t = temperature_raw;",
    "            float res;",
    "            if ((temperature_raw >= tmp->range_min) && (temperature_raw <= tmp->range_max)) {",
    "                const float *w = tmp->w;",
    "                res = w[0] + w[1] * t + w[2] * t*t + w[3] * t*t*t + tmp->offset_ref;",
    "                return res;",
    "            }",
    "        }",
    "    }",
    "",
    "    return 0;",
    "}",
    "",
    "",
    "",
    "",
])

C_STATIC_BUFFERS = (
    "static uint8_t bsec_config[BSEC_MAX_PROPERTY_BLOB_SIZE] = {0};\n"
    "static uint8_t work_buffer[BSEC_MAX_WORKBUFFER_SIZE] = {0};\n"
    "static uint8_t bsec_state[BSEC_MAX_STATE_BLOB_SIZE] = {0};\n"
)

C_PATCHES = [
    Patch(
        "C1",
        "pull in FreeRTOS (vTaskDelay) and the per-demo polverine_cfg.h",
        '#include "bsec_integration.h"\n',
        '#include "bsec_integration.h"\n'
        '#include "freertos/FreeRTOS.h"\n'
        '#include "freertos/task.h"\n'
        "\n"
        '#include "polverine_cfg.h"\n'
        "\n",
    ),
    Patch(
        "C2a",
        "compat: subscribe to BSEC_OUTPUT_BREATH_VOC_EQUIVALENT as index 12",
        "    requested_virtual_sensors[11].sample_rate = sample_rate;\n"
        "\n"
        "    n_requested_virtual_sensors = 12;\n",
        "    requested_virtual_sensors[11].sample_rate = sample_rate;\n"
        "    requested_virtual_sensors[12].sensor_id = BSEC_OUTPUT_BREATH_VOC_EQUIVALENT;\n"
        "    requested_virtual_sensors[12].sample_rate = sample_rate;\n"
        "\n"
        "    n_requested_virtual_sensors = 13;\n",
        group="breath_voc",
    ),
    Patch(
        "C2b",
        "compat: move Bosch's conditional TVOC subscription up to index 13",
        "        requested_virtual_sensors[12].sensor_id = BSEC_OUTPUT_TVOC_EQUIVALENT;\n"
        "        requested_virtual_sensors[12].sample_rate = sample_rate;\n"
        "        n_requested_virtual_sensors = 13;\n",
        "        requested_virtual_sensors[13].sensor_id = BSEC_OUTPUT_TVOC_EQUIVALENT;\n"
        "        requested_virtual_sensors[13].sample_rate = sample_rate;\n"
        "        n_requested_virtual_sensors = 14;\n",
        group="breath_voc",
    ),
    Patch(
        "C2w",
        "cosmetic: drop two trailing-whitespace-only lines in update_subscription()",
        "    uint8_t n_requested_virtual_sensors = NUM_USED_OUTPUTS;\n    \n",
        "    uint8_t n_requested_virtual_sensors = NUM_USED_OUTPUTS;\n\n",
    ),
    Patch(
        "C2w2",
        "cosmetic: same, after n_required_sensor_settings",
        "    uint8_t n_required_sensor_settings = BSEC_MAX_PHYSICAL_SENSOR;\n    \n",
        "    uint8_t n_required_sensor_settings = BSEC_MAX_PHYSICAL_SENSOR;\n\n",
    ),
    Patch(
        "C3",
        "move outputs_t off the BSEC task stack",
        "    outputs_t output;\n",
        "    static outputs_t output;\n",
    ),
    Patch(
        "C4",
        "compat: demux BSEC_OUTPUT_BREATH_VOC_EQUIVALENT into the struct",
        "            case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:\n",
        "            case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:\n"
        "                output.breath_voc_equivalent = bsec_outputs[id].signal;\n"
        "                break;\n"
        "            case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:\n",
        group="breath_voc",
    ),
    Patch(
        "C5a",
        "comment out Bosch's CSV formatting block (open the comment)",
        "    }\n\n    char msg[300] = {0};\n",
        "    }\n/*\n    char msg[300] = {0};\n",
    ),
    Patch(
        "C5b",
        "close that comment and hand the struct, not a string, to the callback",
        "#endif\n    output_ready(msg);\n}\n",
        "#endif\n*/\n    output_ready(&output);\n}\n",
    ),
    Patch(
        "C6",
        "insert the board-specific temperature-offset helper (BlackIoT code)",
        "static uint8_t process_data(int64_t currTimeNs,",
        C_TEMP_OFFSET_HELPER + "static uint8_t process_data(int64_t currTimeNs,",
    ),
    Patch(
        "C7",
        "feed that offset into BSEC_INPUT_HEATSOURCE instead of the static TEMP_OFFSET_*",
        "        inputs[nInputs].sensor_id = BSEC_INPUT_HEATSOURCE;\n",
        "#ifdef BME69X_USE_FPU\n"
        "        extTempOffset = _get_board_specific_temp_offset(data.temperature);\n"
        "#else\n"
        "        extTempOffset = _get_board_specific_temp_offset(data.temperature*0.01f);\n"
        "#endif\n"
        "        inputs[nInputs].sensor_id = BSEC_INPUT_HEATSOURCE;\n",
    ),
    Patch(
        "C8a",
        "hoist the three big BSEC buffers to file scope (several kB off the stack)",
        "return_values_init bsec_iot_init(float sample_rate,",
        C_STATIC_BUFFERS + "\n" + "return_values_init bsec_iot_init(float sample_rate,",
    ),
    Patch(
        "C8b",
        "...and drop their now-duplicate locals in bsec_iot_init()",
        "    uint8_t bsec_config[BSEC_MAX_PROPERTY_BLOB_SIZE] = {0};\n"
        "    uint8_t work_buffer[BSEC_MAX_WORKBUFFER_SIZE] = {0};\n"
        "    uint8_t bsec_state[BSEC_MAX_STATE_BLOB_SIZE] = {0};\n",
        "",
    ),
    Patch(
        "C9",
        "the BME690 hangs off I2C on Polverine, not SPI",
        "bme69x_intf_init(&bme69x[sens_no], BME69X_SPI_INTF, sens_no);",
        "bme69x_intf_init(&bme69x[sens_no], BME69X_I2C_INTF, sens_no);",
    ),
    Patch(
        "C10",
        "bsec_iot_loop() reuses the hoisted statics (memset instead of stack init)",
        "    uint8_t work_buffer[BSEC_MAX_WORKBUFFER_SIZE] = {0};\n"
        "    uint8_t bsec_state[BSEC_MAX_STATE_BLOB_SIZE] = {0};\n"
        "    uint32_t bsec_state_len;\n"
        "    uint64_t time_stamp = 0;\n"
        "    \n",
        "    uint32_t bsec_state_len;\n"
        "    uint64_t time_stamp = 0;\n"
        "\n"
        "    memset(work_buffer, 0, sizeof(work_buffer));\n"
        "    memset(bsec_state, 0, sizeof(bsec_state));\n"
        "   \n",
    ),
    Patch(
        "C11",
        "yield inside the endless loop so the FreeRTOS idle/watchdog tasks can run",
        T + "}\n" + T + "\n"
        "    while (1)\n"
        "    {\n"
        "        for (uint8_t sens_no = 0; sens_no < n_sensors; sens_no++)\n",
        T + "}\n\n"
        "    while (1)\n"
        "    {\n"
        "        vTaskDelay(PLVN_CFG_BSEC_LOOP_DELAY_TIME_MS); // Minimum 1ms yield for ESP32\n"
        "        for (uint8_t sens_no = 0; sens_no < n_sensors; sens_no++)\n",
    ),
]


def read_lf(path):
    with open(path, "rb") as fh:
        raw = fh.read()
    if b"\x00" in raw:
        raise PatchError("%s looks like a binary file" % path)
    txt = raw.decode("latin-1")
    return txt.replace("\r\n", "\n"), ("\r\n" in txt)


def write_eol(path, text, crlf):
    data = (text.replace("\n", "\r\n") if crlf else text).encode("latin-1")
    with open(path, "wb") as fh:
        fh.write(data)


def run(src_dir, dst_dir, breath_voc):
    src_dir = os.path.abspath(src_dir)
    dst_dir = os.path.abspath(dst_dir)
    if not os.path.isdir(src_dir):
        raise PatchError(
            "source directory not found:\n    %s\n\n"
            "Stage BSEC v3.3.0.0 for BME690 first -- see\n"
            "deps/bosch-sensortec/README.md.  The directory must be the\n"
            "bsec_iot_example folder holding the pristine\n"
            "bsec_integration.c / bsec_integration.h." % src_dir
        )
    os.makedirs(dst_dir, exist_ok=True)

    print("source : %s" % src_dir)
    print("output : %s" % dst_dir)
    print("breath-VOC compatibility edits: %s" % ("ON" if breath_voc else "OFF"))
    print("")

    # Patch both files fully in memory first, so a failure on the second file
    # cannot leave a half-migrated pair on disk.
    results = []
    for name, patches in (("bsec_integration.h", H_PATCHES),
                          ("bsec_integration.c", C_PATCHES)):
        src = os.path.join(src_dir, name)
        if not os.path.isfile(src):
            raise PatchError(
                "pristine input not found:\n    %s\n\n"
                "Expected Bosch's BSEC v3.3.0.0 BME690 example directory\n"
                "    <bsec_library_v3.x>/examples/BME690/examples/bsec_iot_example/\n"
                "See deps/bosch-sensortec/README.md." % src
            )
        text, crlf = read_lf(src)
        print("%s:" % name)
        for p in patches:
            if p.group == "breath_voc" and not breath_voc:
                print("  skip  %-5s %s" % (p.pid, p.summary))
                continue
            text = p.apply(text)
            print("  ok    %-5s %s" % (p.pid, p.summary))
        results.append((name, text, crlf))
        print("")

    for name, text, crlf in results:
        out = os.path.join(dst_dir, name)
        write_eol(out, text, crlf)
        print("wrote %s (%s line endings)" % (out, "CRLF" if crlf else "LF"))
    print("")


def main():
    ap = argparse.ArgumentParser(
        description="Apply the Polverine modifications to Bosch's pristine "
                    "BSEC v3.3.0.0 BME690 bsec_integration.{c,h}.")
    ap.add_argument("--src", default=DEFAULT_SRC,
                    help="pristine bsec_iot_example directory "
                         "(default: %(default)s)")
    ap.add_argument("--dst", default=DEFAULT_DST,
                    help="output directory (default: next to this script)")
    ap.add_argument("--with-breath-voc", action="store_true",
                    help="re-add BSEC_OUTPUT_BREATH_VOC_EQUIVALENT. BSEC 3.2.x "
                         "only -- on 3.3.0.0 this makes bsec_iot_init() fail "
                         "with BSEC_E_CONFIG_FEATUREMISMATCH (-35)")
    args = ap.parse_args()
    try:
        run(args.src, args.dst, breath_voc=args.with_breath_voc)
    except PatchError as exc:
        sys.stderr.write("\nERROR: %s\n\nNothing was written.\n" % exc)
        return 2
    print("All patches applied.  Remember: bsec_integration.{c,h} are Bosch-derived")
    print("and are gitignored on purpose -- do not commit them.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
