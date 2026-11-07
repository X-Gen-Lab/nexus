# Real generated C keeps the permanent SRAM owner before runtime construction.
set(boot_platform_bundle "${CMAKE_CURRENT_BINARY_DIR}/qiming-boot-platform")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}"
    --assembly "${NEXUS_SOURCE_DIR}/boards/stm32f407zg_qiming_v41/assemblies/sram-startup.toml"
    --output "${boot_platform_bundle}"
    RESULT_VARIABLE boot_platform_generated
    OUTPUT_VARIABLE boot_platform_output ERROR_VARIABLE boot_platform_error)
if(NOT boot_platform_generated EQUAL 0)
    message(FATAL_ERROR "Boot Platform fixture rejected: ${boot_platform_output}${boot_platform_error}")
endif()
nexus_google_configuration_inputs("${boot_platform_bundle}")
# The reset-proof boundary is mocked; host address placeholders provide the
# generated plan's linker names without claiming physical Flash/RAM placement.
file(STRINGS "${boot_platform_bundle}/bindings.c" boot_platform_linker_rows
    REGEX "^extern uint8_t [A-Za-z_][A-Za-z0-9_]*;$")
set(boot_platform_linker_source "#include <stdint.h>\n")
foreach(row IN LISTS boot_platform_linker_rows)
    string(REGEX REPLACE "^extern " "" definition "${row}")
    string(APPEND boot_platform_linker_source "${definition}\n")
endforeach()
string(APPEND boot_platform_linker_source
    "const uint32_t g_pfnVectors[] = {0U};\n")
file(WRITE "${boot_platform_bundle}/host-linker-symbols.c"
    "${boot_platform_linker_source}")
nexus_google_test(nexus_qiming_boot_platform_test STM32F4_LL
    SOURCES qiming_boot_platform_test.cpp
        "${boot_platform_bundle}/bindings.c"
        "${boot_platform_bundle}/host-linker-symbols.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/fsmc.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/fsmc_boot.c"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Src/stm32f4xx_ll_fsmc.c"
    LIBRARIES Nexus::IO
    INCLUDES "${boot_platform_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private/fsmc_hal"
        "${NEXUS_SOURCE_DIR}/arch/include"
        "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(nexus_qiming_boot_platform_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_qiming_boot_platform_test)
target_compile_definitions(nexus_qiming_boot_platform_test PRIVATE STM32F407xx
    _init=nx_qiming_boot_fixture_init _fini=nx_qiming_boot_fixture_fini)
target_compile_options(nexus_qiming_boot_platform_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/qiming_boot_platform_model.h")
