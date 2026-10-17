# Optional enhanced SPI contracts exercise the production providers directly.
nexus_google_test(nexus_spi_transaction_test
    SOURCES spi_transaction_test.cpp
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/io/native")

nexus_google_test(nexus_stm32_spi_transaction_test STM32F4_LL
    SOURCES spi_transaction_provider_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/spi.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/gpio.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32f407/private")
target_include_directories(nexus_stm32_spi_transaction_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_stm32_spi_transaction_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx NX_SPI_TRANSACTION_STM32)
target_compile_options(nexus_stm32_spi_transaction_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/spi_transaction_model.h")

nexus_google_test(nexus_gd32_spi_transaction_test
    SOURCES spi_transaction_provider_test.cpp
        "${gd_multi_root}/drivers/spi.c"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model/model.c"
    LIBRARIES Nexus::IO
    INCLUDES "${gd_multi_root}" "${gd_multi_root}/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_gd32_spi_transaction_test SYSTEM PRIVATE
    "${gd_multi_vendor}/CMSIS/GD/GD32F4xx/Include"
    "${gd_multi_vendor}/GD32F4xx_standard_peripheral/Include")
target_compile_definitions(nexus_gd32_spi_transaction_test PRIVATE
    GD32F470 HXTAL_VALUE=25000000)
target_compile_options(nexus_gd32_spi_transaction_test PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>"
    "-include${CMAKE_CURRENT_SOURCE_DIR}/spi_transaction_model.h")

nexus_google_test(nexus_stm32_spi_dma_sequence_test STM32F4_LL
    SOURCES spi_dma_sequence_provider_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/spi.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/spi_dma.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/gpio.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32f407/private"
        "${NEXUS_SOURCE_DIR}/io/private")
target_include_directories(nexus_stm32_spi_dma_sequence_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_stm32_spi_dma_sequence_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx NX_SPI_TRANSACTION_STM32)
target_compile_options(nexus_stm32_spi_dma_sequence_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/spi_dma_sequence_model.h")

nexus_google_test(nexus_gd32_spi_dma_sequence_test
    SOURCES spi_dma_sequence_provider_test.cpp
        "${gd_multi_root}/drivers/spi.c"
        "${gd_multi_root}/drivers/spi_dma.c"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model/model.c"
    LIBRARIES Nexus::IO
    INCLUDES "${gd_multi_root}" "${gd_multi_root}/private"
        "${NEXUS_SOURCE_DIR}/io/private"
        "${NEXUS_SOURCE_DIR}/arch/include"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
target_include_directories(nexus_gd32_spi_dma_sequence_test SYSTEM PRIVATE
    "${gd_multi_vendor}/CMSIS/GD/GD32F4xx/Include"
    "${gd_multi_vendor}/GD32F4xx_standard_peripheral/Include")
target_compile_definitions(nexus_gd32_spi_dma_sequence_test PRIVATE
    GD32F470 HXTAL_VALUE=25000000)
target_compile_options(nexus_gd32_spi_dma_sequence_test PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>"
    "-include${CMAKE_CURRENT_SOURCE_DIR}/spi_dma_sequence_model.h")
