# Exact F4 Flash pulses execute against the external register boundary.
set(flash_bundle "${CMAKE_CURRENT_BINARY_DIR}/stm32-flash-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py" --test-support
    --source-root "${NEXUS_SOURCE_DIR}" --assembly
    "${NEXUS_SOURCE_DIR}/tests/contracts/flash_factory/stm32.toml"
    --output "${flash_bundle}" RESULT_VARIABLE flash_generated
    OUTPUT_VARIABLE flash_output ERROR_VARIABLE flash_error)
if(NOT flash_generated EQUAL 0)
    message(FATAL_ERROR "Physical Flash factory rejected: ${flash_output}${flash_error}")
endif()
nexus_google_configuration_inputs("${flash_bundle}")
nexus_google_test(nexus_stm32_flash_program_test
    SOURCES flash_program_test.cpp flash_capability_test.cpp
        "${flash_bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/flash.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/system_lifecycle.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${flash_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu")
target_include_directories(nexus_stm32_flash_program_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
target_compile_definitions(nexus_stm32_flash_program_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx NX_FLASH_PROGRAM_STM32
    _init=nx_flash_fixture_init _fini=nx_flash_fixture_fini)
target_compile_options(nexus_stm32_flash_program_test PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/contracts/flash_program_model.h")
