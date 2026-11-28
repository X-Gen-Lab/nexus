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
            NEXUS_VENDOR_DEVICE_DEFINE STREQUAL "STM32F107xC"
            AND selection STREQUAL "mdio/ETH/clause22"
        )
            continue()
        endif()
        if(
            NEXUS_VENDOR_DEVICE_DEFINE MATCHES "^STM32F10[57]xC$"
            AND selection
                MATCHES
                "^(can/CAN[12]/classic-irq|usb_device/OTG_FS/device-fs|usb_host/OTG_FS/host-fs)$"
        )
            continue()
        endif()
        if(
            NEXUS_VENDOR_DEVICE_DEFINE STREQUAL "STM32F102xB"
            AND selection STREQUAL "usb_device/USB/device-pma"
        )
            continue()
        endif()
        if(
            NEXUS_EXACT_PART
                MATCHES
                "^(STM32F100RBT6B|STM32F103C6T6A|STM32F103Z[EG]T6)$"
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
            NEXUS_EXACT_PART STREQUAL "STM32F103ZET6"
            AND selection
                MATCHES
                "^(block/SDIO/sdio-poll(-write)?|adc/ADC1/simultaneous-scan-dma)$"
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
            NEXUS_VENDOR_DEVICE_DEFINE STREQUAL "STM32F103xE"
            AND selection STREQUAL "audio/I2S2/master-tx"
        )
            continue()
        endif()
        if(
            NEXUS_EXACT_PART STREQUAL "STM32F103ZET6"
            AND selection STREQUAL "advanced_pwm/TIM8/complementary-pwm"
        )
            continue()
        endif()
        if(
            NEXUS_EXACT_PART STREQUAL "STM32F100RBT6B"
            AND selection MATCHES "^pwm/TIM1[567]/fixed-pwm$"
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
nexus_controller_modes(f1_i2c_target i2c_target)
nexus_controller_modes(f1_spi spi)
nexus_controller_modes(f1_adc adc)
nexus_controller_modes(f1_exti exti)
nexus_controller_modes(f1_pwm pwm)
nexus_controller_modes(f1_encoder encoder)
nexus_controller_modes(f1_timer_capture timer_capture)
nexus_controller_modes(f1_advanced_pwm advanced_pwm)
nexus_controller_modes(f1_pwm_adc pwm_adc)
nexus_controller_modes(f1_dac dac)
nexus_controller_modes(f1_audio audio)
nexus_controller_modes(f1_can can)
nexus_controller_modes(f1_mdio mdio)
nexus_controller_modes(f1_usb usb_device)
set(f1_pma_ep1 FALSE)
if("device-pma-bulk" IN_LIST f1_usb OR "device-pma-interrupt" IN_LIST f1_usb)
    set(f1_pma_ep1 TRUE)
endif()
nexus_controller_modes(f1_usb_host usb_host)
nexus_controller_modes(f1_fsmc external_bus)
nexus_controller_modes(f1_sdio block)
if(
    f1_gpio
    OR f1_uart
    OR f1_i2c
    OR f1_i2c_target
    OR f1_spi
    OR f1_adc
    OR f1_exti
    OR f1_pwm
    OR f1_encoder
    OR f1_timer_capture
    OR f1_advanced_pwm
    OR f1_pwm_adc
    OR f1_dac
    OR f1_audio
    OR f1_mdio
    OR f1_fsmc
    OR f1_sdio
    OR "classic-poll-normal" IN_LIST f1_can
    OR "host-fs" IN_LIST f1_usb_host
    OR "device-fs" IN_LIST f1_usb
    OR "device-pma" IN_LIST f1_usb
    OR f1_pma_ep1
)
    target_sources(nexus_soc_stm32f1 PRIVATE drivers/gpio/gpio.c)
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOGPIO)
endif()
if("irq-words" IN_LIST f1_uart)
    target_sources(nexus_soc_stm32f1 PRIVATE drivers/uart/uart_word.c)
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOUARTWord)
endif()
set(f1_uart_byte_modes ${f1_uart})
list(REMOVE_ITEM f1_uart_byte_modes "irq-words")
if(f1_uart_byte_modes)
    target_sources(nexus_soc_stm32f1 PRIVATE drivers/uart/uart.c)
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOUART)
endif()
if("irq-wire" IN_LIST f1_uart)
    # The resolver owns routing. Select only wire code present in this image.
    file(READ "${NEXUS_CONFIG_DIR}/resolved.json" f1_resolved)
    string(JSON f1_controller_count LENGTH "${f1_resolved}" controllers)
    math(EXPR f1_last_controller "${f1_controller_count} - 1")
    set(f1_wire_features)
    foreach(index RANGE "${f1_last_controller}")
        string(JSON kind GET "${f1_resolved}" controllers "${index}" kind)
        string(JSON mode GET "${f1_resolved}" controllers "${index}" mode)
        if(kind STREQUAL "uart" AND mode STREQUAL "irq-wire")
            string(
                JSON protocol
                GET "${f1_resolved}"
                controllers
                "${index}"
                wire
                protocol
            )
            string(REPLACE "-" "_" protocol "${protocol}")
            string(TOUPPER "${protocol}" protocol)
            list(APPEND f1_wire_features "(1U << NX_UART_WIRE_${protocol})")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES f1_wire_features)
    list(JOIN f1_wire_features " | " f1_wire_mask)
    target_compile_definitions(
        nexus_soc_stm32f1
        PRIVATE "NEXUS_STM32F1_UART_WIRE=(${f1_wire_mask})"
    )
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
if("stop-wake" IN_LIST f1_exti)
    if(
        NOT NEXUS_EXACT_PART STREQUAL "STM32F103RBT6"
        OR NOT NEXUS_BACKEND STREQUAL "baremetal"
    )
        message(FATAL_ERROR "STOP wake requires exact F103RB baremetal")
    endif()
    target_sources(nexus_soc_stm32f1 PRIVATE system/stop.c)
    target_compile_definitions(
        nexus_soc_stm32f1
        PRIVATE NX_STM32F1_STOP_OWNER=1
    )
    target_include_directories(
        nexus_soc_stm32f1
        PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include"
        PRIVATE "${NEXUS_CONFIG_DIR}"
    )
    target_link_libraries(nexus_soc_stm32f1 PUBLIC Nexus::ClockDomain)
endif()
add_subdirectory(drivers/pwm)
add_subdirectory(drivers/encoder)
add_subdirectory(drivers/timer_capture)
add_subdirectory(drivers/advanced_pwm)
add_subdirectory(drivers/pwm_adc)
add_subdirectory(drivers/pvd)
add_subdirectory(drivers/can)
add_subdirectory(drivers/ethernet)
add_subdirectory(drivers/sdio)
add_subdirectory(drivers/usb)

# Only the image's startup and generated bindings consume these requirements.
add_library(nexus_stm32f1_startup_usage INTERFACE)
if(f1_pma_ep1)
    target_compile_definitions(
        nexus_stm32f1_startup_usage
        INTERFACE NX_STM32F1_USB_PMA_EP1=1
    )
endif()
if("host-fs" IN_LIST f1_usb_host)
    target_compile_definitions(
        nexus_stm32f1_startup_usage
        INTERFACE NX_STM32F1_USB_HOST=1
    )
    target_include_directories(
        nexus_stm32f1_startup_usage
        INTERFACE
            "${NEXUS_SOURCE_DIR}/soc/stm32/common/dwc_otg_host"
            "${CMAKE_CURRENT_LIST_DIR}/sdk/hal_config/usb"
    )
endif()

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
if("classic-irq" IN_LIST f1_can)
    target_include_directories(
        nexus_stm32f1_startup_usage
        INTERFACE
            "${NEXUS_SOURCE_DIR}/soc/stm32/common/bxcan"
            "${NEXUS_SOURCE_DIR}/io/private"
    )
endif()
if("device-fs" IN_LIST f1_usb)
    target_include_directories(
        nexus_stm32f1_startup_usage
        INTERFACE "${NEXUS_SOURCE_DIR}/soc/stm32/common/dwc_otg_device"
    )
endif()
nexus_use_cmsis_core(nexus_stm32f1_startup_usage)
include("${CMAKE_CURRENT_LIST_DIR}/system/Handoff.cmake")
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
add_subdirectory(drivers/audio)
