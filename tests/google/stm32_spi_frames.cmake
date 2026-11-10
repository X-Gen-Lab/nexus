# One exact production SPI frame owner against external CPU/MMIO boundaries.
nexus_google_test(nexus_stm32_spi_frames_test STM32F4_LL
    SOURCES stm32_spi_frames_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/spi_frames.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/spi_frames_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
target_include_directories(nexus_stm32_spi_frames_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_stm32_spi_frames_test)
target_compile_definitions(nexus_stm32_spi_frames_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx)
target_compile_options(nexus_stm32_spi_frames_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_spi_frames_model.h"
    "$<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_official_model.h>")

# The generated three-instance factory executes the same C hardware owners.
set(spi_frames_bundle "${CMAKE_CURRENT_BINARY_DIR}/f407-spi-frames-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py" --test-support
    --source-root "${NEXUS_SOURCE_DIR}" --assembly
    "${NEXUS_SOURCE_DIR}/tests/contracts/spi_frames_assembly/three-frames-ve.toml"
    --output "${spi_frames_bundle}" RESULT_VARIABLE spi_frames_generated
    OUTPUT_VARIABLE spi_frames_output ERROR_VARIABLE spi_frames_error)
if(NOT spi_frames_generated EQUAL 0)
    message(FATAL_ERROR "SPI frames factory rejected: ${spi_frames_output}${spi_frames_error}")
endif()
nexus_google_configuration_inputs("${spi_frames_bundle}")
nexus_google_test(nexus_stm32_spi_frames_factory_test STM32F4_LL
    SOURCES stm32_spi_frames_factory_test.cpp "${spi_frames_bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/spi_frames.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/spi_frames_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_lifecycle.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_coordinator.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/system_lifecycle.c"
    LIBRARIES Nexus::IO
    INCLUDES "${spi_frames_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_stm32_spi_frames_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
target_compile_definitions(nexus_stm32_spi_frames_factory_test PRIVATE STM32F407xx
    _init=nx_spi_frames_fixture_init _fini=nx_spi_frames_fixture_fini)
target_compile_options(nexus_stm32_spi_frames_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_spi_frames_factory_model.h")
