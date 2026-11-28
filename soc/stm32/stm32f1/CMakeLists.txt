# One package-local build graph for the selected F1 assembly.
# Author: Nexus Team
include("${NEXUS_SOURCE_DIR}/cmake/platform/CMSISCore.cmake")
include("${NEXUS_SOURCE_DIR}/cmake/platform/ControllerModes.cmake")

if(
    NOT NEXUS_CPU_ARCH STREQUAL "cortex-m3"
    OR NOT NEXUS_CPU_FPU STREQUAL "none"
    OR NOT NEXUS_FLOAT_ABI STREQUAL "soft"
    OR NOT NEXUS_VENDOR_DEVICE_DEFINE
        MATCHES
        "^(STM32F100x[BE]|STM32F10[13]x[6BEG]|STM32F102x[6B]|STM32F10[57]xC)$"
)
    message(
        FATAL_ERROR
        "STM32F1 requires a reviewed Cortex-M3 device and soft ABI"
    )
endif()
string(TOLOWER "${NEXUS_VENDOR_DEVICE_DEFINE}" f1_device_stem)
if(
    NOT NEXUS_VENDOR_DEVICE_HEADER STREQUAL "Include/${f1_device_stem}.h"
    OR NOT NEXUS_VENDOR_DEVICE_STARTUP
        STREQUAL
        "Source/Templates/gcc/startup_${f1_device_stem}.s"
)
    message(
        FATAL_ERROR
        "STM32F1 device, header and startup must use the same density"
    )
endif()
if(NOT NEXUS_VENDOR_DEVICE_DEFINE STREQUAL "STM32F103xB")
    foreach(selection IN LISTS NEXUS_SELECTED_CONTROLLERS)
        if(
            NEXUS_VENDOR_DEVICE_DEFINE MATCHES "^STM32F103x[EG]$"
            AND selection STREQUAL "flash/FLASH/poll-halfword"
        )
            continue()
        endif()
        if(
            NEXUS_VENDOR_DEVICE_DEFINE MATCHES "^(STM32F100xB|STM32F103x[EG])$"
            AND selection STREQUAL "dac/DAC/direct"
        )
            continue()
        endif()
        if(
            NEXUS_VENDOR_DEVICE_DEFINE STREQUAL "STM32F103xG"
            AND selection STREQUAL "external_bus/FSMC/async16"
        )
            continue()
        endif()
        if(
            NOT selection
                MATCHES
                "^(gpio/[^/]+/(input|output)|uart/USART2/irq-byte-event)$"
        )
            message(
                FATAL_ERROR
                "${selection} is unavailable for ${NEXUS_VENDOR_DEVICE_DEFINE}"
            )
        endif()
    endforeach()
endif()
set(f1_device "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1")
set(f1_hal "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal")
include("${CMAKE_CURRENT_LIST_DIR}/sdk/Official.cmake")
nexus_stm32f1_official()

add_library(nexus_soc_stm32f1 STATIC system/platform.c)
target_include_directories(
    nexus_soc_stm32f1
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${NEXUS_SOURCE_DIR}/io/private"
)
target_include_directories(
    nexus_soc_stm32f1
    SYSTEM
    PRIVATE "${f1_device}/Include" "${f1_hal}/Inc"
)
target_compile_definitions(
    nexus_soc_stm32f1
    PRIVATE ${NEXUS_VENDOR_DEVICE_DEFINE}
)
target_link_libraries(
    nexus_soc_stm32f1
    PUBLIC nexus_io
    PRIVATE Nexus::Arch Nexus::Core nexus_build_options nexus_owned_warnings
)
nexus_use_cmsis_core(nexus_soc_stm32f1)

nexus_controller_modes(f1_gpio gpio)
nexus_controller_modes(f1_uart uart)
nexus_controller_modes(f1_i2c i2c)
nexus_controller_modes(f1_spi spi)
nexus_controller_modes(f1_adc adc)
nexus_controller_modes(f1_exti exti)
nexus_controller_modes(f1_pwm pwm)
nexus_controller_modes(f1_dac dac)
nexus_controller_modes(f1_can can)
nexus_controller_modes(f1_fsmc external_bus)
if(
    f1_gpio
    OR f1_uart
    OR f1_i2c
    OR f1_spi
    OR f1_adc
    OR f1_exti
    OR f1_pwm
    OR f1_dac
    OR f1_fsmc
    OR "classic-poll-normal" IN_LIST f1_can
)
    target_sources(nexus_soc_stm32f1 PRIVATE drivers/gpio/gpio.c)
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOGPIO)
endif()
if(f1_uart)
    target_sources(nexus_soc_stm32f1 PRIVATE drivers/uart/uart.c)
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOUART)
endif()
if("dma-tx" IN_LIST f1_uart)
    target_sources(nexus_soc_stm32f1 PRIVATE drivers/uart/uart_dma.c)
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IODMA)
endif()
if("rx-dma-idle" IN_LIST f1_uart)
    target_sources(nexus_soc_stm32f1 PRIVATE drivers/uart/uart_rx_dma.c)
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOStream)
endif()
add_subdirectory(drivers/flash)
add_subdirectory(drivers/crc)
add_subdirectory(drivers/identity)
add_subdirectory(drivers/watchdog)
add_subdirectory(drivers/i2c)
add_subdirectory(drivers/spi)
add_subdirectory(drivers/adc)
add_subdirectory(drivers/exti)
add_subdirectory(drivers/pwm)
add_subdirectory(drivers/pvd)
add_subdirectory(drivers/can)

# Only the image's startup and generated bindings consume these requirements.
add_library(nexus_stm32f1_startup_usage INTERFACE)
target_include_directories(
    nexus_stm32f1_startup_usage
    INTERFACE "${CMAKE_CURRENT_LIST_DIR}"
)
target_include_directories(
    nexus_stm32f1_startup_usage
    SYSTEM
    INTERFACE "${f1_device}/Include" "${f1_hal}/Inc"
)
target_compile_definitions(
    nexus_stm32f1_startup_usage
    INTERFACE ${NEXUS_VENDOR_DEVICE_DEFINE}
)
nexus_use_cmsis_core(nexus_stm32f1_startup_usage)
set_property(
    TARGET nexus_soc_stm32f1
    PROPERTY NEXUS_STARTUP_USAGE_TARGET nexus_stm32f1_startup_usage
)
set_property(
    TARGET nexus_soc_stm32f1
    PROPERTY
        NEXUS_STARTUP_SOURCES
            "${f1_device}/${NEXUS_VENDOR_DEVICE_STARTUP}"
            "${CMAKE_CURRENT_LIST_DIR}/system/startup.c"
            "${CMAKE_CURRENT_LIST_DIR}/system/runtime.c"
)
add_subdirectory(drivers/rtc)
add_subdirectory(drivers/dac)
add_subdirectory(drivers/fsmc)
