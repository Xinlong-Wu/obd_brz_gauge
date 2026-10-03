# PC-side LVGL SDL simulator for the obd_brz_gauge firmware UI.
#
# Compiles the UNMODIFIED firmware UI sources (main/export_path, theme_engine,
# obd_data_cache, vehicle_profiles, boot_block_player) against SDL2 + LVGL 8.4.
# All ESP-IDF dependencies are satisfied by the shim layer in shims/ — include
# path shadowing, no firmware file is touched.
#
#   cmake -S simulator -B simulator/build
#   cmake --build simulator/build -j
#   ./simulator/build/obd_gauge_sim --help
cmake_minimum_required(VERSION 3.16)
project(obd_gauge_sim C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)          # gnu11: firmware code relies on GNU cpp features
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(SDL2 REQUIRED)

set(SIM_DIR    "${CMAKE_CURRENT_SOURCE_DIR}")
set(REPO_ROOT  "${SIM_DIR}/..")
set(MAIN_DIR   "${REPO_ROOT}/main")
set(UI_DIR     "${MAIN_DIR}/export_path")

# ---- LVGL: use the exact copy the firmware builds against (managed_components,
#      version pinned by dependencies.lock); fall back to fetching v8.4.0. ----
set(LVGL_DIR "${REPO_ROOT}/managed_components/lvgl__lvgl")
if(NOT EXISTS "${LVGL_DIR}/lvgl.h")
    message(STATUS "managed_components/lvgl not found; fetching lvgl v8.4.0")
    include(FetchContent)
    FetchContent_Declare(lvgl
        GIT_REPOSITORY https://github.com/lvgl/lvgl
        GIT_TAG        v8.4.0)
    FetchContent_MakeAvailable(lvgl)
    set(LVGL_DIR "${lvgl_SOURCE_DIR}")
endif()

file(GLOB_RECURSE LVGL_SOURCES "${LVGL_DIR}/src/*.c")
add_library(lvgl_sim STATIC ${LVGL_SOURCES})
target_include_directories(lvgl_sim PUBLIC
    "${LVGL_DIR}"        # "lvgl.h" and "src/misc/lv_fs.h" (theme_loader.c)
    "${SIM_DIR}"         # lv_conf.h via LV_CONF_INCLUDE_SIMPLE
    "${SIM_DIR}/src")    # sim_clock.h (LV_TICK_CUSTOM calls sim_clock_ms())
target_compile_definitions(lvgl_sim PUBLIC LV_CONF_INCLUDE_SIMPLE)
# Note: LVGL's tick goes through src/sim_clock.c (in the sim binary), so
# lvgl_sim itself no longer needs SDL; the executable links SDL directly
# (see the SDL2 block after add_executable below).

# ---- cJSON: theme manifest parser (theme_loader.c) ----
# cJSON v1.7.18's CMakeLists predates the CMake 4.0 <3.5 removal.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
find_path(CJSON_INCLUDE_DIR cJSON.h)
find_library(CJSON_LIBRARY NAMES cjson libcjson)
if(CJSON_INCLUDE_DIR AND CJSON_LIBRARY)
    message(STATUS "Using system cJSON: ${CJSON_LIBRARY}")
    add_library(cjson_shim INTERFACE)
    target_include_directories(cjson_shim INTERFACE "${CJSON_INCLUDE_DIR}")
    target_link_libraries(cjson_shim INTERFACE "${CJSON_LIBRARY}")
    set(CJSON_EXTRA_INCLUDE "")
else()
    message(STATUS "System cJSON not found; fetching cJSON v1.7.18")
    include(FetchContent)
    set(CJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CJSON_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(CJSON_OVERRIDE_BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
    FetchContent_Declare(cjson
        GIT_REPOSITORY https://github.com/DaveGamble/cJSON
        GIT_TAG        v1.7.18)
    FetchContent_MakeAvailable(cjson)
    add_library(cjson_shim ALIAS cjson)
    # cJSON's own target doesn't reliably propagate its include dir through
    # the alias — obd_gauge_sim adds it explicitly after target creation.
    set(CJSON_EXTRA_INCLUDE "${cjson_SOURCE_DIR}")
endif()

# ---- Firmware UI sources (compiled unmodified) ----
file(GLOB SIM_SCREENS "${UI_DIR}/screens/*.c")
file(GLOB SIM_FONTS   "${UI_DIR}/fonts/*.c")
file(GLOB SIM_IMAGES  "${UI_DIR}/images/*.c")
# NOTE: do NOT copy the file list from export_path/filelist.txt / its
# CMakeLists — those reference assets that no longer exist; the actual
# directory listing is the truth.
set(FW_SOURCES
    "${UI_DIR}/ui.c"
    "${UI_DIR}/ui_ext.c"
    "${UI_DIR}/ui_disp_item.c"
    "${UI_DIR}/ui_theme.c"
    "${UI_DIR}/ui_helpers.c"
    "${UI_DIR}/ui_theme_generated.c"
    ${SIM_SCREENS}
    ${SIM_FONTS}
    ${SIM_IMAGES}
    "${MAIN_DIR}/theme_engine/theme_loader.c"
    "${MAIN_DIR}/theme_engine/theme_test.c"
    "${MAIN_DIR}/app_obd_dsp/obd_data_cache.c"
    "${MAIN_DIR}/app_obd_dsp/vehicle_profiles.c"
    "${MAIN_DIR}/app_obd_dsp/boot_block_player.c")

file(GLOB SHIM_SOURCES "${SIM_DIR}/shims/src/*.c")

file(GLOB SIM_SOURCES "${SIM_DIR}/src/*.c")

add_executable(obd_gauge_sim
    ${SIM_SOURCES}
    ${SHIM_SOURCES}
    ${FW_SOURCES})

# shims/include MUST come first: it shadows the ESP-IDF headers (esp_log.h,
# freertos/*, driver/gpio.h, bsp_obd_dsp/lcd_driver/ST77916.h, ...) that the
# real SDK would normally provide.
target_include_directories(obd_gauge_sim BEFORE PRIVATE
    "${SIM_DIR}/shims/include")
target_include_directories(obd_gauge_sim PRIVATE
    "${SIM_DIR}"           # lv_conf.h (LV_CONF_INCLUDE_SIMPLE)
    "${SIM_DIR}/src"       # sim_platform.h / cli.h / fake_data.h
    "${MAIN_DIR}"          # "bsp_obd_dsp/...", "app_obd_dsp/...", "theme_engine/..."
    "${UI_DIR}")           # ui.c's "ui.h" etc.
if(CJSON_EXTRA_INCLUDE)
    target_include_directories(obd_gauge_sim PRIVATE "${CJSON_EXTRA_INCLUDE}")
endif()

target_compile_definitions(obd_gauge_sim PRIVATE
    OBD_GAUGE_BUILD_TAG="sim"
    SIM_REPO_ROOT="${REPO_ROOT}")

target_link_libraries(obd_gauge_sim PRIVATE
    lvgl_sim
    cjson_shim)

# SDL2: used by the sim sources (window, events, ticks in real mode).
if(TARGET SDL2::SDL2)
    target_link_libraries(obd_gauge_sim PRIVATE SDL2::SDL2)
else()
    target_include_directories(obd_gauge_sim PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(obd_gauge_sim PRIVATE ${SDL2_LIBRARIES})
endif()
