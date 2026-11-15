# One package-local CMake graph owns selected implementations and SDK objects.
include("${NEXUS_SOURCE_DIR}/cmake/platform/CMSISCore.cmake")
include("${NEXUS_SOURCE_DIR}/cmake/platform/ControllerModes.cmake")
set(stm32_package_dir "${CMAKE_CURRENT_LIST_DIR}")
set(stm32_device "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4")
nexus_cmsis_core_include(stm32_cmsis)
if(NOT NEXUS_STM32_DEVICE OR NOT NEXUS_STM32_STARTUP)
    message(FATAL_ERROR "STM32F4 requires resolved exact device and startup facts")
endif()
string(TOLOWER "${NEXUS_STM32_DEVICE}" stm32_device_header)
foreach(required "${stm32_device}/Include/${stm32_device_header}.h"
                 "${stm32_cmsis}/core_cm4.h"
                 "${stm32_device}/Source/Templates/gcc/${NEXUS_STM32_STARTUP}")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Missing pinned official STM32 SDK input: ${required}")
    endif()
endforeach()
add_library(nexus_soc_stm32f4 STATIC)
set_property(TARGET nexus_soc_stm32f4 PROPERTY NEXUS_STM32_DEVICE "${NEXUS_STM32_DEVICE}")
target_link_libraries(nexus_soc_stm32f4 PUBLIC nexus_io
    PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
target_include_directories(nexus_soc_stm32f4 PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}" "${NEXUS_SOURCE_DIR}/io/private")
target_include_directories(nexus_soc_stm32f4 PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
set(stm32_ip_includes
    "${CMAKE_CURRENT_LIST_DIR}/drivers/ip/i2c_v1_async"
    "${CMAKE_CURRENT_LIST_DIR}/drivers/ip/classic_can"
    "${CMAKE_CURRENT_LIST_DIR}/drivers/ip/dwc_otg_device"
    "${CMAKE_CURRENT_LIST_DIR}/drivers/ip/legacy_mac"
    "${CMAKE_CURRENT_LIST_DIR}/drivers/ip/advanced_timer")
target_include_directories(nexus_soc_stm32f4 PRIVATE ${stm32_ip_includes})
target_include_directories(nexus_soc_stm32f4 SYSTEM PRIVATE "${stm32_device}/Include")
nexus_use_cmsis_core(nexus_soc_stm32f4)
target_compile_definitions(nexus_soc_stm32f4 PRIVATE ${NEXUS_STM32_DEVICE})
include("${CMAKE_CURRENT_LIST_DIR}/sdk/CMakeLists.txt")
nexus_stm32f4_official(nexus_soc_stm32f4)
set(stm32_gpio_helpers OFF)
set(stm32_uart_hal OFF)
set(stm32_sai_hal OFF)
foreach(module identity gpio uart spi i2c flash watchdog exti timer adc dac crc
               crypto graphics can usb ethernet sdio rtc memory capture audio entropy)
    include("${CMAKE_CURRENT_LIST_DIR}/drivers/${module}/CMakeLists.txt")
endforeach()
if(stm32_gpio_helpers)
    target_sources(nexus_soc_stm32f4 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/drivers/gpio/gpio.c")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/system/CMakeLists.txt")
