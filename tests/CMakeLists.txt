# 主机端单元测试(CMake CTest,零测试框架依赖)—— M0 测试地基。
#
#   cmake -S tests -B tests/build && cmake --build tests/build -j
#   ctest --test-dir tests/build --output-on-failure
#
# 复用 simulator/shims 的 ESP-IDF 头遮蔽与桩实现编译固件纯逻辑模块;
# LVGL 用与固件同版本的 managed_components 副本(缺失时 FetchContent
# v8.4.0),配 tests/lv_conf.h(无 SDL tick)。CI 见 .github/workflows/ci.yml。
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

# ---- LVGL(与固件/模拟器同版本;tests/lv_conf.h,无 SDL) ----
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
add_library(lvgl_tests STATIC ${LVGL_SOURCES})
target_include_directories(lvgl_tests PUBLIC
    "${LVGL_DIR}"
    "${TESTS_DIR}")
target_compile_definitions(lvgl_tests PUBLIC LV_CONF_INCLUDE_SIMPLE)

# ---- cJSON(theme_loader.c 的 manifest 解析;同 simulator 的发现逻辑) ----
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
find_path(CJSON_INCLUDE_DIR cJSON.h)
find_library(CJSON_LIBRARY NAMES cjson libcjson)
if(CJSON_INCLUDE_DIR AND CJSON_LIBRARY)
    add_library(cjson_tests INTERFACE)
    target_include_directories(cjson_tests INTERFACE "${CJSON_INCLUDE_DIR}")
    target_link_libraries(cjson_tests INTERFACE "${CJSON_LIBRARY}")
else()
    message(STATUS "System cJSON not found; fetching cJSON v1.7.18")
    include(FetchContent)
    set(CJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CJSON_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(CJSON_OVERRIDE_BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
    FetchContent_Declare(cjson
        GIT_REPOSITORY https://github.com/DaveGamble/cJSON
        GIT_TAG        v1.7.18)
    # EXCLUDE_FROM_ALL:cJSON 自带的测试目标不进默认构建(1.7.18 无视
    # CJSON_BUILD_TESTS 开关),CTest 也用 -R "^test_" 只跑本仓库用例。
    FetchContent_GetProperties(cjson)
    if(NOT cjson_POPULATED)
        FetchContent_Populate(cjson)
        add_subdirectory("${cjson_SOURCE_DIR}" "${cjson_BINARY_DIR}" EXCLUDE_FROM_ALL)
    endif()
    add_library(cjson_tests INTERFACE)
    target_include_directories(cjson_tests INTERFACE "${cjson_SOURCE_DIR}")
    target_link_libraries(cjson_tests INTERFACE cjson)
endif()

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
        "${MAIN_DIR}/export_path")
    target_link_libraries("${name}" PRIVATE lvgl_tests cjson_tests)
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

# ---- 车型表:数量 / 越界钳制 / 数据自洽 ----
# (set_active 内部会触碰数据缓存,一并编入)
add_gauge_test(test_vehicle_profiles
    SOURCES test_vehicle_profiles.c
            "${MAIN_DIR}/app_obd_dsp/vehicle_profiles.c"
            "${MAIN_DIR}/app_obd_dsp/obd_data_cache.c"
            ${TEST_SHIMS})

# ---- 数据项系统:有效性判定 / 格式化 / 自然量程 / 元数据访问 ----
add_gauge_test(test_ui_disp_item
    SOURCES test_ui_disp_item.c
            "${MAIN_DIR}/export_path/ui_disp_item.c"
            ${TEST_SHIMS})

# ---- 主题引擎:默认回退 + theme.bin v1 加载(manifest/颜色/页面) ----
add_gauge_test(test_theme_engine
    SOURCES test_theme_engine.c
            stub_ui_theme.c
            "${MAIN_DIR}/theme_engine/theme_loader.c"
            "${MAIN_DIR}/theme_engine/theme_test.c"
            "${SHIM_DIR}/src/esp_partition_file.c"
            ${TEST_SHIMS})
