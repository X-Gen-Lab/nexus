# Exercise the actual STM32F103ZE SD host and block protocol without a socket.
# Author: Nexus Team
nexus_google_test(
    stm32f103_sdio_contracts
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/transport_test.cpp"
        "${f1_dir}/drivers/sdio/transport.c"
        "${f1_dir}/drivers/sdio/sd_block.c"
        "${f1_dir}/drivers/gpio/gpio.c"
    INCLUDES "${f1_dir}" "${NEXUS_SOURCE_DIR}/io/private"
    LIBRARIES Nexus::Arch Nexus::Core Nexus::IOBlock
)
nexus_use_cmsis_core(stm32f103_sdio_contracts)
target_include_directories(
    stm32f103_sdio_contracts
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(
    stm32f103_sdio_contracts
    PRIVATE STM32F103xE NX_STM32F1_SDIO_4BIT=1 NX_STM32F1_SDIO_RX_DMA=1
)
target_compile_options(
    stm32f103_sdio_contracts
    PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/model.h"
)
