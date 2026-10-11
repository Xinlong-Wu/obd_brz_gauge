# 主机端单元测试(CMake CTest,零测试框架依赖)—— M0 测试地基。
#
#   cmake -S tests -B tests/build && cmake --build tests/build -j
#   ctest --test-dir tests/build --output-on-failure
#
# 复用 simulator/shims 的 ESP-IDF 头遮蔽与桩实现编译固件纯逻辑模块;
# LVGL 用与固件同版本的 managed_components 副本(缺失时 FetchContent
# v9.6.0),配 tests/lv_conf.h(无 SDL tick)。CI 见 .github/workflows/ci.yml。
cmake_minimum_required(VERSION 3.16)
project(obd_gauge_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(TESTS_DIR   "${CMAKE_CURRENT_SOURCE_DIR}")
set(REPO_ROOT   "${TESTS_DIR}/..")
set(MAIN_DIR    "${REPO_ROOT}/main")
set(SIM_DIR     "${REPO_ROOT}/simulator")
set(SHIM_DIR    "${SIM_DIR}/shims")
set(UI_DIR      "${REPO_ROOT}/ui/JDM")

# ---- LVGL(与固件/模拟器同版本;tests/lv_conf.h,无 SDL) ----
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
add_library(lvgl_tests STATIC ${LVGL_SOURCES})
target_include_directories(lvgl_tests PUBLIC
    "${LVGL_DIR}"          # lvgl.h shim + lvgl_private.h
    "${LVGL_DIR}/include"  # v9 public headers
    "${TESTS_DIR}")
target_compile_definitions(lvgl_tests PUBLIC LV_CONF_INCLUDE_SIMPLE)

# ---- 共同编译环境:固件头 + shim 头(遮蔽 ESP-IDF) ----
function(add_gauge_test name)
    cmake_parse_arguments(T "" "" "SOURCES" ${ARGN})
    add_executable("${name}" ${T_SOURCES})
    target_include_directories("${name}" BEFORE PRIVATE
        "${SHIM_DIR}/include")
    target_include_directories("${name}" PRIVATE
        "${TESTS_DIR}"
        "${SIM_DIR}/src"        # sim_platform.h / cli.h(nvs_storage_mock 依赖)
        "${MAIN_DIR}"
        "${UI_DIR}")            # "core/jdm_xxx.h"
    target_link_libraries("${name}" PRIVATE lvgl_tests)
    add_test(NAME "${name}" COMMAND "${name}")
    set_tests_properties("${name}" PROPERTIES WORKING_DIRECTORY "${REPO_ROOT}")
endfunction()

# shim 子集:不含 bsp_stubs.c(依赖 lv_timer+SDL 无关但引 BLE API)与
# ota_boot_stubs.c(bootmedia 文件后端,单测用不到)。
set(TEST_SHIMS
    "${SHIM_DIR}/src/esp_stubs.c"
    "${SHIM_DIR}/src/freertos_stubs.c"
    "${SHIM_DIR}/src/heap_caps_shim.c"
    "${SHIM_DIR}/src/app_event_stub.c"
    "${SHIM_DIR}/src/strlcat_port.c"
    "${SHIM_DIR}/src/nvs_storage_mock.c")

enable_testing()

# ---- 数据缓存:通道读写 / 无效哨兵 / RPM 覆盖层 / 挡位推算 ----
add_gauge_test(test_obd_data_cache
    SOURCES test_obd_data_cache.c
            "${MAIN_DIR}/app_obd_dsp/obd_data_cache.c"
            "${MAIN_DIR}/app_obd_dsp/vehicle_profiles.c"
            ${TEST_SHIMS})

# ---- ZC6 CAN 监听帧解码:挡位直读 / G 力 / 胎压(纯逻辑头,无 .c 依赖) ----
add_gauge_test(test_zc6_monitor_decode
    SOURCES test_zc6_monitor_decode.c)

# ---- OBD 轮询档位:间隔映射 / 越界回退 / mock 写读往返 ----
add_gauge_test(test_nvs_poll_mode
    SOURCES test_nvs_poll_mode.c
            ${TEST_SHIMS})

# ---- JDM 数据通道:哨兵判定 / 元数据量程表 / 纯逻辑映射 ----
add_gauge_test(test_jdm_data
    SOURCES test_jdm_data.c
            "${UI_DIR}/core/jdm_data.c"
            "${MAIN_DIR}/app_obd_dsp/obd_data_cache.c"
            "${MAIN_DIR}/app_obd_dsp/vehicle_profiles.c"
            ${TEST_SHIMS})

# ---- JDM readout 格式化纯逻辑:通道除数约定 / 负值 / 无效哨兵 ----
add_gauge_test(test_jdm_widgets
    SOURCES test_jdm_widgets.c)

# ---- JDM 通道报警阈值:两级转速 / 温度 / 油压 / 增压 / 电压 / 胎压 / G 值 ----
add_gauge_test(test_jdm_alerts
    SOURCES test_jdm_alerts.c)

# ---- JDM 字体表:设计系统阶梯 / 分辨率换算 / init 有效性 ----
# 字体 blob(生成物,~400KB C 数组)一并编入
file(GLOB JDM_FONT_BLOBS "${UI_DIR}/assets/fonts/*.c")
add_gauge_test(test_jdm_fonts
    SOURCES test_jdm_fonts.c
            "${UI_DIR}/core/jdm_fonts.c"
            ${JDM_FONT_BLOBS})

# ---- 用户自定义仪表页数据模型(旧 M4 遗留,NVS 兼容清洗) ----
add_gauge_test(test_ui_dashboard
    SOURCES test_ui_dashboard.c
            ${TEST_SHIMS})

# ---- 错误日志环形缓冲:sanitize / append 回绕饱和 / mock API ----
add_gauge_test(test_nvs_error_log
    SOURCES test_nvs_error_log.c
            ${TEST_SHIMS})

# ---- 车型表:数量 / 越界钳制 / 数据自洽 ----
# (set_active 内部会触碰数据缓存,一并编入)
add_gauge_test(test_vehicle_profiles
    SOURCES test_vehicle_profiles.c
            "${MAIN_DIR}/app_obd_dsp/vehicle_profiles.c"
            "${MAIN_DIR}/app_obd_dsp/obd_data_cache.c"
            ${TEST_SHIMS})
