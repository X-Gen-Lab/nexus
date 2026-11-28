cmake_minimum_required(VERSION 3.31)
project(nexus_stm32f1_wwdg_early_warning_reference LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(NEXUS_ASSEMBLY_FILE
    "${CMAKE_CURRENT_LIST_DIR}/rb.toml"
    CACHE FILEPATH
    "Watchdog provider fixture assembly"
)

if(NEXUS_SOURCE_ROOT)
    set(Nexus_DIR "${NEXUS_SOURCE_ROOT}/cmake")
endif()
find_package(Nexus CONFIG REQUIRED)
nexus_add_platform(ASSEMBLY "${NEXUS_ASSEMBLY_FILE}")
nexus_add_firmware(nexus_stm32f1_wwdg_early_warning_consumer SOURCES main.c)
nexus_target_warnings(nexus_stm32f1_wwdg_early_warning_consumer STRICT)
target_link_libraries(
    nexus_stm32f1_wwdg_early_warning_consumer
    PRIVATE Nexus::STM32F103RBWatchdogWarning
)

find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
get_target_property(fixture_source Nexus::Config NEXUS_SOURCE_DIR)
get_target_property(fixture_config Nexus::Platform NEXUS_CONFIG_DIR)
set(retention_check "${fixture_source}/tools/measurement/retention.py")
set(retention_report "${CMAKE_CURRENT_BINARY_DIR}/bin/provider-retention.json")
set_property(
    TARGET nexus_stm32f1_wwdg_early_warning_consumer
    APPEND
    PROPERTY LINK_DEPENDS "${retention_check}"
)
add_custom_command(
    TARGET nexus_stm32f1_wwdg_early_warning_consumer
    POST_BUILD
    BYPRODUCTS "${retention_report}"
    COMMAND
        "${Python3_EXECUTABLE}" -B "${retention_check}" --elf
        "$<TARGET_FILE:nexus_stm32f1_wwdg_early_warning_consumer>" --bindings
        "${fixture_config}/bindings.c" --resolved
        "${fixture_config}/resolved.json" --output "${retention_report}"
    VERBATIM
)
