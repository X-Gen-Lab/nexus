# One package-local CMake graph owns selected implementations and SDK objects.
include("${NEXUS_SOURCE_DIR}/cmake/platform/CMSISCore.cmake")
include("${NEXUS_SOURCE_DIR}/cmake/platform/ControllerModes.cmake")
set(stm32_package_dir "${CMAKE_CURRENT_LIST_DIR}")
set(stm32_device "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4")
nexus_cmsis_core_include(stm32_cmsis)
set(NEXUS_STM32_DEVICE "${NEXUS_VENDOR_DEVICE_DEFINE}")
if(
    NOT NEXUS_STM32_DEVICE
    OR NOT NEXUS_VENDOR_DEVICE_HEADER
    OR NOT NEXUS_VENDOR_DEVICE_STARTUP
)
    message(
        FATAL_ERROR
        "STM32F4 requires resolved exact device and startup facts"
    )
endif()
foreach(
    required
    "${stm32_device}/${NEXUS_VENDOR_DEVICE_HEADER}"
    "${stm32_cmsis}/core_cm4.h"
    "${stm32_device}/${NEXUS_VENDOR_DEVICE_STARTUP}"
)
    if(NOT EXISTS "${required}")
        message(
            FATAL_ERROR
            "Missing pinned official STM32 SDK input: ${required}"
        )
    endif()
endforeach()
add_library(nexus_soc_stm32f4 STATIC)
set_property(
    TARGET nexus_soc_stm32f4
    PROPERTY NEXUS_STM32_DEVICE "${NEXUS_STM32_DEVICE}"
)
target_link_libraries(
    nexus_soc_stm32f4
    PUBLIC nexus_io
    PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings
)
target_include_directories(
    nexus_soc_stm32f4
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${NEXUS_SOURCE_DIR}/io/private"
)
target_include_directories(
    nexus_soc_stm32f4
    PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include"
)
set(stm32_ip_includes
    "${CMAKE_CURRENT_LIST_DIR}/drivers/ip/i2c_v1_async"
    "${CMAKE_CURRENT_LIST_DIR}/../common/bxcan"
    "${CMAKE_CURRENT_LIST_DIR}/../common/dwc_otg_device"
    "${CMAKE_CURRENT_LIST_DIR}/../common/dwc_otg_host"
    "${CMAKE_CURRENT_LIST_DIR}/drivers/ip/legacy_mac"
    "${CMAKE_CURRENT_LIST_DIR}/drivers/ip/advanced_timer"
)
target_include_directories(nexus_soc_stm32f4 PRIVATE ${stm32_ip_includes})
target_include_directories(
    nexus_soc_stm32f4
    SYSTEM
    PRIVATE "${stm32_device}/Include"
)
nexus_use_cmsis_core(nexus_soc_stm32f4)
target_compile_definitions(nexus_soc_stm32f4 PRIVATE ${NEXUS_STM32_DEVICE})
include("${CMAKE_CURRENT_LIST_DIR}/sdk/CMakeLists.txt")
nexus_stm32f4_official(nexus_soc_stm32f4)
# Startup and bindings consume this private contract, never application sources.
add_library(nexus_stm32f4_startup_usage INTERFACE)
target_include_directories(
    nexus_stm32f4_startup_usage
    INTERFACE "${stm32_package_dir}" ${stm32_ip_includes}
)
target_include_directories(
    nexus_stm32f4_startup_usage
    SYSTEM
    INTERFACE
        "${stm32_device}/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Inc"
)
target_compile_definitions(
    nexus_stm32f4_startup_usage
    INTERFACE ${NEXUS_STM32_DEVICE}
)
nexus_use_cmsis_core(nexus_stm32f4_startup_usage)
set_property(
    TARGET nexus_soc_stm32f4
    PROPERTY NEXUS_STARTUP_USAGE_TARGET nexus_stm32f4_startup_usage
)

# Shared implementation is selected by its users, independently of module order.
function(nexus_stm32f4_gpio target)
    get_property(
        selected
        TARGET "${target}"
        PROPERTY NEXUS_STM32F4_GPIO_SELECTED
    )
    if(NOT selected)
        target_sources(
            "${target}"
            PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/drivers/gpio/gpio.c"
        )
        set_property(TARGET "${target}" PROPERTY NEXUS_STM32F4_GPIO_SELECTED ON)
    endif()
endfunction()
foreach(
    module
    identity
    gpio
    uart
    spi
    i2c
    flash
    watchdog
    exti
    timer
    adc
    pwm_adc
    dac
    crc
    crypto
    graphics
    can
    usb
    ethernet
    sdio
    rtc
    memory
    capture
    audio
    entropy
)
    include("${CMAKE_CURRENT_LIST_DIR}/drivers/${module}/CMakeLists.txt")
endforeach()
include("${CMAKE_CURRENT_LIST_DIR}/system/CMakeLists.txt")
