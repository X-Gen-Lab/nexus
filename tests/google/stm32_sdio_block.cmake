# Generic public dispatch and one exact private polling socket owner.
nexus_google_test(nexus_block_port_test SOURCES block_port_test.cpp
    LIBRARIES Nexus::IO)

nexus_google_test(nexus_stm32_sdio_block_test STM32F4_LL
    SOURCES stm32_sdio_block_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/sdio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/platform_cold.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private"
        "${NEXUS_SOURCE_DIR}/arch/include"
        "${NEXUS_SOURCE_DIR}/tests/google"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
target_include_directories(nexus_stm32_sdio_block_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
target_compile_definitions(nexus_stm32_sdio_block_test PRIVATE STM32F407xx)
set(sdio_model "${CMAKE_CURRENT_SOURCE_DIR}/stm32_sdio_block_model.h")
target_compile_options(nexus_stm32_sdio_block_test PRIVATE "-include${sdio_model}")
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32SDIO.cmake")
nexus_stm32f407_sdio_ll(nexus_stm32_sdio_block_test)
set(sdio_vendor nexus_stm32_sdio_block_test_sdio_ll)
target_include_directories(${sdio_vendor} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(${sdio_vendor} BEFORE PRIVATE "-include${sdio_model}")
# Keep pinned target-address width diagnostics visible in the host-only TU.
target_compile_options(${sdio_vendor} PRIVATE
    -Wno-error=pointer-to-int-cast -Wno-error=int-to-pointer-cast)
nexus_use_cmsis_host_model(${sdio_vendor})
target_link_options(nexus_stm32_sdio_block_test PRIVATE -Wl,--gc-sections)
