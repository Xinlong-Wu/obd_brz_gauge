file(GLOB_RECURSE SOURCES_C *.c)
file(GLOB_RECURSE SOURCES_CPP *.cpp)

# ---- UI theme table codegen ------------------------------------------------
# themes/registry.txt + themes/*/<id>/theme.toml -> export_path/ui_theme_generated.c
# Runs BEFORE idf_component_register because SRC_DIRS is globbed at register time.
# Adding a theme requires editing registry.txt, and registry.txt is watched
# below, so a new theme always triggers a reconfigure and regeneration.
set(THEMES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../themes")
file(GLOB THEME_MANIFESTS "${THEMES_DIR}/*/*/theme.toml")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${THEMES_DIR}/registry.txt" ${THEME_MANIFESTS})

# idf.py sets `python` to the IDF virtualenv interpreter; fall back for plain
# cmake / IDE reconfigures. gen_themes.py has no third-party dependencies.
if(NOT DEFINED python)
    set(python "python3")
endif()

execute_process(
    COMMAND "${python}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/gen_themes.py"
    RESULT_VARIABLE THEME_GEN_RESULT
    OUTPUT_VARIABLE THEME_GEN_STDOUT
    ERROR_VARIABLE  THEME_GEN_STDERR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)
if(NOT THEME_GEN_RESULT EQUAL 0)
    message(FATAL_ERROR
            "Theme generation failed.\n${THEME_GEN_STDOUT}\n${THEME_GEN_STDERR}\n"
            "See themes/README.md.")
endif()
if(THEME_GEN_STDERR)
    message(WARNING "${THEME_GEN_STDERR}")
endif()
message(STATUS "${THEME_GEN_STDOUT}")

# ---- UI image assets codegen -----------------------------------------------
# assets_src/images/*.png masters -> export_path/images{,_r240,_r466}/*.c
# (LANCZOS to each render resolution; byte-identical data at 360).
# Pillow is imported lazily only when a master's SHA changes.
set(ASSET_MASTERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets_src/images")
file(GLOB ASSET_MASTERS "${ASSET_MASTERS_DIR}/*.png")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ASSET_MASTERS})

execute_process(
    COMMAND "${python}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/gen_assets.py"
    RESULT_VARIABLE ASSET_GEN_RESULT
    OUTPUT_VARIABLE ASSET_GEN_STDOUT
    ERROR_VARIABLE  ASSET_GEN_STDERR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)
if(NOT ASSET_GEN_RESULT EQUAL 0)
    message(FATAL_ERROR
            "Image asset generation failed.\n${ASSET_GEN_STDOUT}\n${ASSET_GEN_STDERR}")
endif()
if(ASSET_GEN_STDOUT)
    message(STATUS "${ASSET_GEN_STDOUT}")
endif()

# Build metadata for app-side compatibility checks and on-device manifests.
set(OBD_GAUGE_GIT_HASH "unknown")
set(OBD_GAUGE_GIT_BRANCH "unknown")
set(OBD_GAUGE_GIT_COUNT 0)

execute_process(
    COMMAND git -C "${CMAKE_CURRENT_SOURCE_DIR}/.." rev-parse --short=12 HEAD
    RESULT_VARIABLE OBD_GAUGE_GIT_HASH_RESULT
    OUTPUT_VARIABLE OBD_GAUGE_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT OBD_GAUGE_GIT_HASH_RESULT EQUAL 0 OR OBD_GAUGE_GIT_HASH STREQUAL "")
    set(OBD_GAUGE_GIT_HASH "unknown")
endif()

execute_process(
    COMMAND git -C "${CMAKE_CURRENT_SOURCE_DIR}/.." rev-parse --abbrev-ref HEAD
    RESULT_VARIABLE OBD_GAUGE_GIT_BRANCH_RESULT
    OUTPUT_VARIABLE OBD_GAUGE_GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT OBD_GAUGE_GIT_BRANCH_RESULT EQUAL 0 OR OBD_GAUGE_GIT_BRANCH STREQUAL "")
    set(OBD_GAUGE_GIT_BRANCH "unknown")
endif()

execute_process(
    COMMAND git -C "${CMAKE_CURRENT_SOURCE_DIR}/.." rev-list --count HEAD
    RESULT_VARIABLE OBD_GAUGE_GIT_COUNT_RESULT
    OUTPUT_VARIABLE OBD_GAUGE_GIT_COUNT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT OBD_GAUGE_GIT_COUNT_RESULT EQUAL 0 OR OBD_GAUGE_GIT_COUNT STREQUAL "")
    set(OBD_GAUGE_GIT_COUNT 0)
endif()

string(REGEX REPLACE "[^A-Za-z0-9._-]" "_" OBD_GAUGE_GIT_BRANCH_SAFE "${OBD_GAUGE_GIT_BRANCH}")
set(OBD_GAUGE_BUILD_TAG "${OBD_GAUGE_GIT_BRANCH_SAFE}-${OBD_GAUGE_GIT_COUNT}-${OBD_GAUGE_GIT_HASH}")

# Board layer: every boards/*.c always compiles and self-guards with
# #if CONFIG_OBD_BOARD_* — the component CMakeLists runs in the requirements
# phase where CONFIG_ variables are not injected yet, so CMake-level gating
# is not possible here (see board_dispatch.c for the dispatch pattern).
# The WS185 driver dirs also always compile: OBD_HW_VERSION keeps its default
# selection on WS175 builds, the unused LCD/touch/exio code is never called
# there and is dropped by the linker's --gc-sections.
idf_component_register(SRC_DIRS
                    "."
                    "export_path/components"
                    "export_path/fonts"
                    "export_path/images"
                    "export_path/images_r240"
                    "export_path/images_r466"
                    "export_path/theme_assets"
                    "export_path/screens"
                    "export_path"
                    "app_obd_dsp"
                    "bsp_obd_dsp"
                    "bsp_obd_dsp/boards"
                    "bsp_obd_dsp/lcd_driver"
                    "bsp_obd_dsp/lcd_driver/esp_lcd_st77916"
                    "bsp_obd_dsp/exio"
                    "bsp_obd_dsp/i2c_driver"
                    "bsp_obd_dsp/touch_driver"
                    "theme_engine"
                    INCLUDE_DIRS
                    "."
                    REQUIRES
                    esp_http_server
                    esp_wifi
                    bt
                    mbedtls
                    nvs_flash
                    app_update
                    json
                    esp_lcd
                    driver)

# Auto-generate the factory bootmedia image in the raw layout the firmware
# reads (manifest at partition offset 0, frame stream at 0x1000 — see
# app_obd_dsp/boot_media_mount.c); flashed together with idf.py flash.
set(BOOTMEDIA_SLOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../bootmedia/slot_a)
# Emit at the build root so the documented flashing path build/bootmedia.bin
# (docs/FLASH.md, host esptool flows) stays valid.
set(BOOTMEDIA_IMG ${CMAKE_BINARY_DIR}/bootmedia.bin)
add_custom_command(
    OUTPUT ${BOOTMEDIA_IMG}
    COMMAND ${PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/gen_bootmedia.py
            ${BOOTMEDIA_SLOT_DIR} ${BOOTMEDIA_IMG}
    DEPENDS ${BOOTMEDIA_SLOT_DIR}/boot_block.txt ${BOOTMEDIA_SLOT_DIR}/boot_block.bin
    COMMENT "Generating raw bootmedia image"
    VERBATIM)
add_custom_target(bootmedia_img DEPENDS ${BOOTMEDIA_IMG})
add_dependencies(${COMPONENT_LIB} bootmedia_img)
partition_table_get_partition_info(bootmedia_offset "--partition-name bootmedia" offset)
esptool_py_flash_target_image(flash bootmedia "${bootmedia_offset}" "${BOOTMEDIA_IMG}")

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    OBD_GAUGE_GIT_HASH="${OBD_GAUGE_GIT_HASH}"
    OBD_GAUGE_GIT_BRANCH="${OBD_GAUGE_GIT_BRANCH_SAFE}"
    OBD_GAUGE_GIT_COUNT=${OBD_GAUGE_GIT_COUNT}
    OBD_GAUGE_BUILD_TAG="${OBD_GAUGE_BUILD_TAG}")
