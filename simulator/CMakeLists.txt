# PC-side LVGL SDL simulator for the obd_brz_gauge JDM UI.
#
# Compiles the UNMODIFIED firmware UI sources (ui/JDM, obd_data_cache,
# vehicle_profiles) against SDL2 + LVGL 9.6. All ESP-IDF dependencies are
# satisfied by the shim layer in shims/ — include path shadowing, no firmware
# file is touched.
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
set(UI_DIR     "${REPO_ROOT}/ui/JDM")

# ---- LVGL: use the exact copy the firmware builds against (managed_components,
#      version pinned by dependencies.lock); fall back to fetching v9.6.0. ----
set(LVGL_DIR "${REPO_ROOT}/managed_components/lvgl__lvgl")
if(NOT EXISTS "${LVGL_DIR}/lvgl.h")
    message(STATUS "managed_components/lvgl not found; fetching lvgl v9.6.0")
    include(FetchContent)
    FetchContent_Declare(lvgl
        GIT_REPOSITORY https://github.com/lvgl/lvgl
        GIT_TAG        v9.6.0)
    FetchContent_MakeAvailable(lvgl)
    set(LVGL_DIR "${lvgl_SOURCE_DIR}")
endif()

file(GLOB_RECURSE LVGL_SOURCES "${LVGL_DIR}/src/*.c")
add_library(lvgl_sim STATIC ${LVGL_SOURCES})
target_include_directories(lvgl_sim PUBLIC
    "${LVGL_DIR}"          # lvgl.h shim + lvgl_private.h (in-tree lv_meter widget)
    "${LVGL_DIR}/include"  # v9 public headers
    "${SIM_DIR}"           # lv_conf.h via LV_CONF_INCLUDE_SIMPLE
    "${SIM_DIR}/src")      # sim_clock.h
target_compile_definitions(lvgl_sim PUBLIC LV_CONF_INCLUDE_SIMPLE)
# Note: LVGL 9 takes its tick source via lv_tick_set_cb() (wired to the sim
# clock in main.c), so lvgl_sim itself no longer needs SDL; the executable
# links SDL directly (see the SDL2 block after add_executable below).

# ---- Firmware UI sources (compiled unmodified) ----
file(GLOB JDM_WIDGET_SOURCES "${UI_DIR}/widgets/*.c")
file(GLOB JDM_FONT_BLOBS "${UI_DIR}/assets/fonts/*.c")
set(FW_SOURCES
    "${UI_DIR}/jdm_ui.c"
    "${UI_DIR}/core/jdm_theme.c"
    "${UI_DIR}/core/jdm_fonts.c"
    "${UI_DIR}/core/jdm_data.c"
    "${UI_DIR}/core/jdm_nav.c"
    ${JDM_WIDGET_SOURCES}
    ${JDM_FONT_BLOBS}
    "${MAIN_DIR}/app_obd_dsp/obd_data_cache.c"
    "${MAIN_DIR}/app_obd_dsp/vehicle_profiles.c")

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
    "${MAIN_DIR}"          # "bsp_obd_dsp/...", "app_obd_dsp/..."
    "${UI_DIR}")           # "core/jdm_xxx.h", "jdm_ui.h"

target_compile_definitions(obd_gauge_sim PRIVATE
    OBD_GAUGE_BUILD_TAG="sim"
    SIM_REPO_ROOT="${REPO_ROOT}")

target_link_libraries(obd_gauge_sim PRIVATE
    lvgl_sim)

# SDL2: used by the sim sources (window, events, ticks in real mode).
if(TARGET SDL2::SDL2)
    target_link_libraries(obd_gauge_sim PRIVATE SDL2::SDL2)
else()
    target_include_directories(obd_gauge_sim PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(obd_gauge_sim PRIVATE ${SDL2_LIBRARIES})
endif()
