cmake_minimum_required(VERSION 3.31)
project(gpio_exti_provider_contract LANGUAGES C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(NEXUS_BUILD_TESTS OFF CACHE BOOL "No host tests in ARM consumer")
set(NEXUS_BUILD_WORKLOAD OFF CACHE BOOL "Explicit public consumer main")
add_subdirectory("${NEXUS_SOURCE_ROOT}" nexus-sdk)
nexus_add_firmware(gpio_exti_contract SOURCES main.c)
nexus_target_warnings(gpio_exti_contract STRICT)
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
get_target_property(fixture_config Nexus::Platform NEXUS_CONFIG_DIR)
add_custom_command(TARGET gpio_exti_contract POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_ROOT}/tools/measurement/retention.py"
        --elf "$<TARGET_FILE:gpio_exti_contract>"
        --bindings "${fixture_config}/bindings.c"
        --output "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json"
    VERBATIM)
