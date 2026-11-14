cmake_minimum_required(VERSION 3.31)
project(nexus_gd32_reference_tests LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NEXUS_INSTALLED_SOURCE)
    set(NEXUS_SOURCE_ROOT "${NEXUS_INSTALLED_SOURCE}")
    if(NOT NEXUS_EXPECTED_SOURCE_REVISION)
        message(FATAL_ERROR "Installed host reference requires an exact SDK revision")
    endif()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_ROOT}/cmake/package/package_source_sdk.py" --verify "${NEXUS_SOURCE_ROOT}"
        RESULT_VARIABLE gd_sdk_status OUTPUT_VARIABLE gd_sdk_identity ERROR_VARIABLE gd_sdk_error)
    if(NOT gd_sdk_status EQUAL 0)
        message(FATAL_ERROR "Installed SDK identity rejected: ${gd_sdk_error}")
    endif()
    string(JSON gd_sdk_revision GET "${gd_sdk_identity}" source_revision)
    string(JSON gd_sdk_publishable GET "${gd_sdk_identity}" publishable)
    if(NOT gd_sdk_publishable OR NOT gd_sdk_revision STREQUAL NEXUS_EXPECTED_SOURCE_REVISION)
        message(FATAL_ERROR "Installed host SDK is not the expected publishable revision")
    endif()
    if(NOT NEXUS_GTEST_ROOT OR NOT NEXUS_CMSIS_HOST_ROOT)
        message(FATAL_ERROR "Installed host reference requires explicit external test support")
    endif()
else()
    get_filename_component(NEXUS_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
    set(NEXUS_GTEST_ROOT "${NEXUS_SOURCE_ROOT}/ext/googletest")
    set(NEXUS_CMSIS_HOST_ROOT "${NEXUS_SOURCE_ROOT}/tests/cmsis_host")
endif()
set(NEXUS_SOURCE_DIR "${NEXUS_SOURCE_ROOT}")
set(NEXUS_TEST_SUPPORT ON)
set(NEXUS_GD32_VERIFY_ONLY ON)
include("${CMAKE_CURRENT_LIST_DIR}/../CMakeLists.txt")
unset(NEXUS_GD32_VERIFY_ONLY)
set(gd_sdk "${gd_supplier}/Firmware")
add_subdirectory("${NEXUS_GTEST_ROOT}" googletest EXCLUDE_FROM_ALL)
add_executable(nexus_gd32_reference_test driver_test.cpp
    ../gpio.c ../uart.c
    "${NEXUS_SOURCE_ROOT}/core/src/request.c"
    "${NEXUS_SOURCE_ROOT}/core/src/time.c"
    "${NEXUS_SOURCE_ROOT}/io/src/api/gpio.c"
    "${NEXUS_SOURCE_ROOT}/io/src/api/uart.c"
    "${gd_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_gpio.c"
    "${gd_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_usart.c"
    "${gd_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_rcu.c")
target_compile_definitions(nexus_gd32_reference_test PRIVATE GD32F470 NEXUS_CMSIS_HOST_MODEL=1)
target_include_directories(nexus_gd32_reference_test PRIVATE ..
    "${NEXUS_SOURCE_ROOT}/core/include"
    "${NEXUS_SOURCE_ROOT}/io/include"
    "${NEXUS_SOURCE_ROOT}/arch/include"
    "${NEXUS_CMSIS_HOST_ROOT}")
target_include_directories(nexus_gd32_reference_test SYSTEM PRIVATE
    "${gd_sdk}/CMSIS/GD/GD32F4xx/Include"
    "${gd_sdk}/GD32F4xx_standard_peripheral/Include"
    "${NEXUS_SOURCE_ROOT}/vendors/arm/CMSIS_6/CMSIS/Core/Include")
target_compile_options(nexus_gd32_reference_test PRIVATE
    -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow
    -Os -ffunction-sections -fdata-sections
    "-include${NEXUS_CMSIS_HOST_ROOT}/compiler.h"
    "-include${CMAKE_CURRENT_LIST_DIR}/model.h"
    "$<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes;-Wmissing-prototypes;-Wno-int-to-pointer-cast>")
target_link_options(nexus_gd32_reference_test PRIVATE -Wl,--gc-sections)
target_link_libraries(nexus_gd32_reference_test PRIVATE GTest::gmock_main)
enable_testing()
add_test(NAME gd32_reference COMMAND nexus_gd32_reference_test)
# The unmodified 32-bit SDK uses unsigned-long masks. LP64 host register models
# retain the SDK bytes but do not promote that host-only conversion warning.
set_source_files_properties(
    "${gd_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_gpio.c"
    "${gd_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_usart.c"
    "${gd_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_rcu.c"
    PROPERTIES COMPILE_OPTIONS "-Wno-conversion")

add_executable(nexus_gd32_system_test system_test.cpp ../system.c
    "${NEXUS_SOURCE_ROOT}/core/src/fault.c"
    "${gd_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_gpio.c"
    "${gd_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_rcu.c"
    "${gd_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_timer.c")
target_compile_definitions(nexus_gd32_system_test PRIVATE GD32F470
    NEXUS_CMSIS_HOST_MODEL=1 NX_GD32_GPIO_SELECTED=1 NX_GD32_UART_SELECTED=1)
target_include_directories(nexus_gd32_system_test PRIVATE ..
    "${NEXUS_SOURCE_ROOT}/core/include" "${NEXUS_SOURCE_ROOT}/io/include"
    "${NEXUS_SOURCE_ROOT}/arch/include" "${NEXUS_CMSIS_HOST_ROOT}")
target_include_directories(nexus_gd32_system_test SYSTEM PRIVATE
    "${gd_sdk}/CMSIS/GD/GD32F4xx/Include"
    "${gd_sdk}/GD32F4xx_standard_peripheral/Include"
    "${NEXUS_SOURCE_ROOT}/vendors/arm/CMSIS_6/CMSIS/Core/Include")
target_compile_options(nexus_gd32_system_test PRIVATE
    -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow
    -Os -ffunction-sections -fdata-sections
    "-include${NEXUS_CMSIS_HOST_ROOT}/compiler.h"
    "-include${CMAKE_CURRENT_LIST_DIR}/system_model.h"
    "$<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes;-Wmissing-prototypes;-Wno-int-to-pointer-cast>")
target_link_options(nexus_gd32_system_test PRIVATE -Wl,--gc-sections
    -Wl,--wrap=timer_interrupt_flag_clear)
target_link_libraries(nexus_gd32_system_test PRIVATE GTest::gmock_main)
add_test(NAME gd32_system COMMAND nexus_gd32_system_test)
set_source_files_properties(
    "${gd_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_timer.c"
    PROPERTIES COMPILE_OPTIONS "-Wno-conversion")
