# Optional enhanced SPI contracts exercise the production providers directly.
nexus_google_test(nexus_spi_transaction_test
    SOURCES spi_transaction_test.cpp
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/native/drivers")

nexus_google_test(nexus_stm32_spi_transaction_test STM32F4_LL
    SOURCES spi_transaction_provider_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/spi.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/gpio.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private")
target_include_directories(nexus_stm32_spi_transaction_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_stm32_spi_transaction_test)
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
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/spi.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/spi_dma.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/gpio.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private"
        "${NEXUS_SOURCE_DIR}/io/private")
target_include_directories(nexus_stm32_spi_dma_sequence_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_stm32_spi_dma_sequence_test)
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

# Owner contracts reuse each family's exact provider graph and register model.
function(nexus_spi_dma_owner_contract name reference)
    get_property(sources TARGET ${reference} PROPERTY SOURCES)
    get_property(libraries TARGET ${reference} PROPERTY LINK_LIBRARIES)
    get_property(includes TARGET ${reference} PROPERTY INCLUDE_DIRECTORIES)
    get_property(system_includes TARGET ${reference}
        PROPERTY SYSTEM_INCLUDE_DIRECTORIES)
    get_property(definitions TARGET ${reference} PROPERTY COMPILE_DEFINITIONS)
    get_property(options TARGET ${reference} PROPERTY COMPILE_OPTIONS)
    set(owned_includes)
    set(sdk_includes)
    foreach(directory IN LISTS includes)
        string(FIND "${directory}" "${NEXUS_SOURCE_DIR}/vendors/" sdk_prefix)
        if(sdk_prefix EQUAL 0)
            list(APPEND sdk_includes "${directory}")
        else()
            list(APPEND owned_includes "${directory}")
        endif()
    endforeach()
    nexus_google_test(${name} SOURCES ${sources}
        LIBRARIES ${libraries} Nexus::SPIDMATransactionOwner
        INCLUDES ${owned_includes})
    target_include_directories(${name} SYSTEM PRIVATE
        ${system_includes} ${sdk_includes})
    target_compile_definitions(${name} PRIVATE
        ${definitions} NX_SPI_DMA_OWNER_TEST)
    target_compile_options(${name} PRIVATE ${options})
endfunction()
nexus_spi_dma_owner_contract(nexus_stm32_spi_dma_owner_test
    nexus_stm32_spi_dma_sequence_test)
nexus_spi_dma_owner_contract(nexus_gd32_spi_dma_owner_test
    nexus_gd32_spi_dma_sequence_test)
