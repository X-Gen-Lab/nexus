# Explicit spi behavior and private model dependencies.
# Author: Nexus Team
nexus_google_test(nexus_stm32_spi_dma_test STM32F4_LL
    SOURCES "${nexus_google_root}/soc/stm32/stm32f4/spi/stm32_spi_dma_test.cpp"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/spi/spi.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio/gpio.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4"
)

target_include_directories(
    nexus_stm32_spi_dma_test
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}"
)

nexus_use_cmsis_core(nexus_stm32_spi_dma_test)

target_compile_definitions(
    nexus_stm32_spi_dma_test
    PRIVATE NEXUS_STM32_MODEL STM32F407xx
)

target_sources(
    nexus_stm32_spi_dma_test
    PRIVATE "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/spi/spi_dma.c"
)

target_include_directories(
    nexus_stm32_spi_dma_test
    PRIVATE "${NEXUS_SOURCE_DIR}/io/private"
)

include("${nexus_google_root}/soc/stm32/stm32f4/spi/spi_sequence_factory.cmake")

include("${nexus_google_root}/soc/stm32/stm32f4/spi/stm32_spi_frames.cmake")
