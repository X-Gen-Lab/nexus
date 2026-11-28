cmake_minimum_required(VERSION 3.31)
project(nexus_f103_rtc_fixture LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(NEXUS_ASSEMBLY_FILE
    "${CMAKE_CURRENT_LIST_DIR}/rb.toml"
    CACHE FILEPATH
    "F103 RTC retained backup fixture assembly"
)

if(NEXUS_SOURCE_ROOT)
    set(Nexus_DIR "${NEXUS_SOURCE_ROOT}/cmake")
endif()
find_package(Nexus CONFIG REQUIRED)
option(
    NEXUS_F1_RAW_ALARM_PROBE
    "Build the fixed F103RB private RTC alarm reference"
    OFF
)
if(NEXUS_F1_RAW_ALARM_PROBE)
    nexus_add_platform(ASSEMBLY "${CMAKE_CURRENT_LIST_DIR}/raw-alarm.toml")
    get_target_property(fixture_source Nexus::Config NEXUS_SOURCE_DIR)
    set(f1_dir "${fixture_source}/soc/stm32/stm32f1")
    nexus_add_firmware(
        nexus_f103_rtc_alarm
        SOURCES raw_alarm.c "${f1_dir}/drivers/rtc/counter.c"
    )
    nexus_target_warnings(nexus_f103_rtc_alarm STRICT)
    nexus_use_cmsis_core(nexus_f103_rtc_alarm)
    target_include_directories(
        nexus_f103_rtc_alarm
        PRIVATE "${f1_dir}" "${fixture_source}/io/private"
    )
    target_include_directories(
        nexus_f103_rtc_alarm
        SYSTEM
        PRIVATE "${fixture_source}/vendors/st/cmsis_device_f1/Include"
    )
    target_compile_definitions(
        nexus_f103_rtc_alarm
        PRIVATE STM32F103xB NX_STM32F1_RTC_RAW_ALARM
    )
    return()
endif()

nexus_add_platform(ASSEMBLY "${NEXUS_ASSEMBLY_FILE}")
nexus_add_firmware(nexus_f103_rtc_consumer SOURCES main.c)
nexus_target_warnings(nexus_f103_rtc_consumer STRICT)

find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
get_target_property(fixture_source Nexus::Config NEXUS_SOURCE_DIR)
get_target_property(fixture_config Nexus::Platform NEXUS_CONFIG_DIR)
set(retention_check "${fixture_source}/tools/measurement/retention.py")
set(retention_report "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json")
set_property(
    TARGET nexus_f103_rtc_consumer
    APPEND
    PROPERTY LINK_DEPENDS "${retention_check}"
)
add_custom_command(
    TARGET nexus_f103_rtc_consumer
    POST_BUILD
    BYPRODUCTS "${retention_report}"
    COMMAND
        "${Python3_EXECUTABLE}" -B "${retention_check}" --elf
        "$<TARGET_FILE:nexus_f103_rtc_consumer>" --bindings
        "${fixture_config}/bindings.c" --resolved
        "${fixture_config}/resolved.json" --output "${retention_report}"
    VERBATIM
)
