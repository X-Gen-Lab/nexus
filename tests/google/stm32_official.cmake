# The production F407 entry points execute against official register structs.
# Only silicon driver translation units receive the host CPU primitive port.
# Shared targets may also contain GD CPU-model sources; their SDK stays isolated.
foreach(driver gpio uart uart_dma spi spi_dma i2c timer watchdog adc adc_stream
               adc_continuous)
    set_property(SOURCE "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/${driver}.c"
        APPEND PROPERTY COMPILE_OPTIONS
        "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_official_model.h")
endforeach()

nexus_google_test(nexus_stm32_official_test STM32F4_LL SOURCES stm32_official_test.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/gpio.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/uart.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private")
target_include_directories(nexus_stm32_official_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_stm32_official_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx)

nexus_google_test(nexus_stm32_watchdog_test STM32F4_LL SOURCES stm32_watchdog_test.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/watchdog.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private")
target_include_directories(nexus_stm32_watchdog_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_stm32_watchdog_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx)

# Fixed-address mapping is an explicit Linux register reference requirement.
# The selected target executes all cases in every normal Native Google gate.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(hal_reference "${NEXUS_SOURCE_DIR}/tests/contracts/stm32_hal_reference")
    nexus_google_test(nexus_stm32_hal_reference
        SOURCES "${hal_reference}/reference_test.cpp")
    include("${hal_reference}/reference.cmake")
    nexus_stm32_hal_reference(nexus_stm32_hal_reference)
endif()
