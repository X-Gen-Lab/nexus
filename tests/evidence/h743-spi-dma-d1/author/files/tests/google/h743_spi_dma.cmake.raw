# One complete finite contract program with real private DMA hardware.
nexus_google_test(nexus_h743_spi_dma_test SOURCES h743_spi_dma_test.cpp
    h743_spi_dma_model.cpp h743_model.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/gpio.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/spi_wire.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/spi_dma.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32h743/private")
target_include_directories(nexus_h743_spi_dma_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_h743_spi_dma_test PRIVATE STM32H743xx)
target_compile_options(nexus_h743_spi_dma_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_spi_dma_model.h")
