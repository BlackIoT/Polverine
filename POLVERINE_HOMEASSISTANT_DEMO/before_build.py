# check for depedencies files
import os
import sys

from SCons.Script import Import


Import("env")


# NOTE ON PATH ARITHMETIC -- DO NOT "FIX" THE MISSING '$' BELOW.
# env.subst("PROJECT_DIR") (no '$') deliberately returns the literal string
# "PROJECT_DIR", which acts as a throw-away path component cancelled by the
# first '..' in the join.  SCons runs this script with the project directory as
# CWD, so abspath("PROJECT_DIR/../" + "../deps/...") == <repo-root>/deps/... .
# Writing "$PROJECT_DIR" would make prj_dir absolute and move every check one
# directory ABOVE the repository, so every check would fail.

def create_dummy_file(file_path:str):
    os.makedirs(os.path.dirname(file_path), exist_ok=True)  # Ensure the directory exists
    with open(file_path, 'w') as f:
        f.write("dummy\n")  # Optional: You can write default content
def check_deps(prj_dir:str):
    # Define the list of files to check
    deps = [
        {
            'name':'BMV080 SDK r.11.2.0',
            'files':[
                '../deps/bosch-sensortec/BMV080-SDK/api/lib/xtensa_esp32s3/xtensa_esp32s3_elf_gcc/release/lib_bmv080.a',
                '../deps/bosch-sensortec/BMV080-SDK/api/lib/xtensa_esp32s3/xtensa_esp32s3_elf_gcc/release/lib_postProcessor.a',
                '../deps/bosch-sensortec/BMV080-SDK/api/inc/bmv080.h',
                '../deps/bosch-sensortec/BMV080-SDK/api/inc/bmv080_defs.h',
                '../deps/bosch-sensortec/BMV080-SDK/api_examples/_common/inc/bmv080_example.h'
                ],

            'wheretoget':'https://www.bosch-sensortec.com/software-tools/double-opt-in-forms/sdk-v11-2.html',
            'howto':'Unzip the BMV080 SDK straight into deps/bosch-sensortec/BMV080-SDK/ -- the zip has no top folder to rename:\n'
                    '  unzip -q <bmv080-sdk-*.zip> -d deps/bosch-sensortec/BMV080-SDK\n'
                    'That directory must then directly contain api/ and LICENSE.md.\n'
                    'See deps/bosch-sensortec/README.md for the full staging procedure.'
        },
        {
            'name':'BSEC3 Library r.3.3.0.0',
            'files': [
                '../deps/bosch-sensortec/bsec_library_v3.x/release_bin/IAQ/bin/esp/esp32_s3/libalgobsec.a',
                '../deps/bosch-sensortec/bsec_library_v3.x/release_bin/IAQ/inc/bsec_datatypes.h',
                '../deps/bosch-sensortec/bsec_library_v3.x/release_bin/IAQ/inc/bsec_interface.h',
                '../deps/bosch-sensortec/bsec_library_v3.x/release_bin/IAQ/config/bme690/bme690_iaq_33v_3s_4d/bsec_iaq.h',
                '../deps/bosch-sensortec/bsec_library_v3.x/release_bin/IAQ/config/bme690/bme690_iaq_33v_3s_4d/bsec_iaq.c',
                '../deps/bosch-sensortec/bsec_library_v3.x/examples/BME690/examples/bsec_iot_example/bsec_integration.c',
                '../deps/bosch-sensortec/bsec_library_v3.x/examples/BME690/examples/bsec_iot_example/bsec_integration.h',
                ],
            'wheretoget':'https://www.bosch-sensortec.com/software-tools/double-opt-in-forms/bsec-software-3-3-0-0-form.html',
            'howto':'Unzip BSEC 3.3.0.0 straight into deps/bosch-sensortec/bsec_library_v3.x/ -- the zip has no top folder to rename:\n'
                    '  unzip -q <bsec_v3-3-0-0.zip> -d deps/bosch-sensortec/bsec_library_v3.x\n'
                    'That folder must be the ROOT of the zip, i.e. it must directly contain release_bin/ and examples/.\n'
                    'NOTE: BSEC 3.2.1.0 and older used algo/bsec_IAQ/ instead of release_bin/IAQ/ and will NOT work.\n'
                    'See deps/bosch-sensortec/README.md for the full staging procedure.'
        },
        {
            'name':'Polverine BSEC integration wrapper',
            'files': [
                '../deps/bosch-sensortec/bsec_library_wrappers/bsec_integration.c',
                '../deps/bosch-sensortec/bsec_library_wrappers/bsec_integration.h',
                ],
            'wheretoget':'https://www.bosch-sensortec.com/software-tools/double-opt-in-forms/bsec-software-3-3-0-0-form.html',
            'howto':'These files derive from Bosch example code and cannot be redistributed, so they are NOT in\n'
                    'this repository.  Generate them from your own BSEC 3.3.0.0 download (staged as above) by\n'
                    'running, from anywhere:\n'
                    '    python3 deps/bosch-sensortec/bsec_library_wrappers/apply_polverine_patches.py\n'
                    'That script applies the Polverine modifications to the pristine Bosch files and writes the\n'
                    'result into deps/bosch-sensortec/bsec_library_wrappers/.  Do not commit the result.\n'
                    'See deps/bosch-sensortec/README.md.'
        },
    ]

    # Check if each file exists
    for dep in deps:
        name = dep['name']
        where = dep['wheretoget']
        files = dep['files']
        howto = dep.get('howto', 'After the zip file is downloaded, extract it to the right location so that the above file could be located')

        for file in files:
            file_path_amend = os.path.join(prj_dir, '../' + file)
            file_path_full = os.path.abspath(file_path_amend)
            if not os.path.exists(file_path_full):
                #create_dummy_file(file_path_amend)
                print('*' * 160)
                print(f"Error: Missing file '{file_path_full}' from the {name} which can be get from:<{where}>")
                print(howto)
                print('*' * 160)
                sys.exit(1)  # Abort the build if any file is missing



def before_build(source, target, env):
    check_deps(env.subst("PROJECT_DIR"))

print("Current CLI targets", COMMAND_LINE_TARGETS)
print("Current Build targets", BUILD_TARGETS)
before_build(None, None, env)

# Register the function to be called before the build process
#env.AddPreAction("build", before_build)
