# Exercise the selected STM32F1 SPI byte and transaction owners.
# Author: Nexus Team
set(f1_spi_dir "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
foreach(family IN ITEMS stm32f103 stm32f107)
    set(target "${family}_spi_contracts")
    if(family STREQUAL "stm32f107")
        set(selector STM32F107xC)
    else()
        set(selector STM32F103xB)
    endif()
    nexus_google_test(
        ${target}
        SOURCES
            "${CMAKE_CURRENT_LIST_DIR}/spi_test.cpp"
            "${f1_spi_dir}/drivers/gpio/gpio.c"
            "${f1_spi_dir}/drivers/spi/spi.c"
        INCLUDES "${f1_spi_dir}" "${NEXUS_SOURCE_DIR}/io/private"
        LIBRARIES Nexus::Arch Nexus::Core Nexus::IOSPI
    )
    nexus_use_cmsis_core(${target})
    target_include_directories(
        ${target}
        SYSTEM
        PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
            "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
    )
    target_compile_definitions(${target} PRIVATE "${selector}")
    target_compile_options(
        ${target}
        PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/spi_model.h"
    )
endforeach()
