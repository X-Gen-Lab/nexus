# CMake owns source dependencies; these domains add no runtime dispatch.
add_library(nexus_io_contract INTERFACE)
target_include_directories(nexus_io_contract INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_io_contract INTERFACE nexus_core)

function(nexus_io_domain name)
    cmake_parse_arguments(IO "" "ALIAS" "SOURCES;DEPENDS" ${ARGN})
    if(IO_UNPARSED_ARGUMENTS OR NOT IO_ALIAS OR NOT IO_SOURCES)
        message(FATAL_ERROR "An I/O domain requires explicit sources and alias")
    endif()
    add_library(nexus_io_${name} STATIC EXCLUDE_FROM_ALL ${IO_SOURCES})
    target_include_directories(nexus_io_${name} PRIVATE
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/private")
    target_link_libraries(nexus_io_${name} PUBLIC nexus_io_contract ${IO_DEPENDS}
        PRIVATE nexus_build_options nexus_owned_warnings Nexus::Arch)
    add_library(Nexus::${IO_ALIAS} ALIAS nexus_io_${name})
endfunction()

nexus_io_domain(wake ALIAS IOWake SOURCES src/mechanisms/wake.c)
nexus_io_domain(dma ALIAS IODMA SOURCES src/mechanisms/dma.c)
nexus_io_domain(stream ALIAS IOStream
    SOURCES src/mechanisms/frame_stream.c src/mechanisms/stream.c
        src/mechanisms/stream_budget.c
    DEPENDS nexus_io_dma nexus_io_wake)
nexus_io_domain(stream_profile_storage ALIAS IOStreamProfileStorage
    SOURCES src/mechanisms/stream_profile_storage.c DEPENDS nexus_io_stream)
nexus_io_domain(adc_format ALIAS IOADCFormat
    SOURCES src/mechanisms/adc_format.c)
nexus_io_domain(gpio ALIAS IOGPIO SOURCES src/api/gpio.c src/api/pin_state.c)
nexus_io_domain(uart ALIAS IOUART
    SOURCES src/api/uart.c src/api/uart_word.c src/mechanisms/uart_rs485.c
        src/mechanisms/stream_profile.c
    DEPENDS nexus_io_stream_profile_storage nexus_io_gpio)
nexus_io_domain(spi ALIAS IOSPI
    SOURCES src/api/spi.c src/mechanisms/spi_dma_owner_binding.c
        src/mechanisms/spi_dma_sequence.c src/mechanisms/spi_dma_transaction.c
        src/mechanisms/spi_transaction.c
    DEPENDS nexus_io_stream)
nexus_io_domain(i2c ALIAS IOI2C
    SOURCES src/api/i2c.c src/mechanisms/i2c_bus_clear.c
        src/mechanisms/i2c_long.c
    DEPENDS nexus_io_dma nexus_io_wake)
nexus_io_domain(flash ALIAS IOFlash
    SOURCES src/api/flash.c src/mechanisms/flash_region.c)
nexus_io_domain(watchdog ALIAS IOWatchdog SOURCES src/api/watchdog.c)
nexus_io_domain(exti ALIAS IOEXTI SOURCES src/api/exti.c DEPENDS nexus_io_wake)
nexus_io_domain(pwm ALIAS IOTimer SOURCES src/api/timer.c)
nexus_io_domain(adc ALIAS IOADC
    SOURCES src/api/adc.c src/mechanisms/adc_observer.c
        src/mechanisms/adc_stream_profile.c
    DEPENDS nexus_io_stream_profile_storage nexus_io_adc_format)
nexus_io_domain(can ALIAS IOCAN SOURCES src/api/can.c DEPENDS nexus_io_wake)
nexus_io_domain(usb_device ALIAS IOUSBDevice
    SOURCES src/api/usb_device.c src/mechanisms/usb_profile.c
    DEPENDS nexus_io_wake)
nexus_io_domain(mdio ALIAS IOMDIO SOURCES src/api/mdio.c)
nexus_io_domain(eth ALIAS IOEthernet SOURCES src/api/ethernet.c
    DEPENDS nexus_io_mdio nexus_io_dma nexus_io_wake)
nexus_io_domain(eth_frames ALIAS IOEthernetFrames SOURCES src/api/ethernet_frames.c
    DEPENDS nexus_io_eth)
nexus_io_domain(timer_capture ALIAS IOTimerCapture SOURCES src/api/timer_capture.c
    DEPENDS nexus_io_wake)
nexus_io_domain(advanced_pwm ALIAS IOAdvancedPWM SOURCES src/api/advanced_pwm.c
    DEPENDS nexus_io_wake)
nexus_io_domain(pwm_group ALIAS IOPWMGroup SOURCES src/api/pwm_group.c
    DEPENDS nexus_io_wake)
nexus_io_domain(encoder ALIAS IOEncoder SOURCES src/api/encoder.c)
nexus_io_domain(pwm_adc ALIAS IOPWMADC SOURCES src/api/pwm_adc.c
    DEPENDS nexus_io_adc)
nexus_io_domain(can_fd ALIAS IOCANFD SOURCES src/api/can_fd.c
    DEPENDS nexus_io_wake)
nexus_io_domain(adc_group ALIAS IOADCGroup SOURCES src/api/adc_group.c
    DEPENDS nexus_io_adc)
nexus_io_domain(rtc ALIAS IORTC SOURCES src/api/rtc.c DEPENDS nexus_io_wake)
nexus_io_domain(lptim ALIAS IOLPTIM SOURCES src/api/lptim.c DEPENDS nexus_io_wake)
nexus_io_domain(entropy ALIAS IOEntropy SOURCES src/api/entropy.c)

# Common observation is usable without a physical controller. Every other
# source is compiled only through a selected domain or explicit consumer target.
add_library(nexus_io STATIC
    src/mechanisms/diagnostic.c src/mechanisms/observation.c)
target_link_libraries(nexus_io PUBLIC nexus_io_contract
    PRIVATE nexus_build_options nexus_owned_warnings Nexus::Arch)
set(_nexus_io_domains ${NEXUS_SELECTED_KINDS})
# Runtime and Native admit caller-defined providers, independently of Board facts.
if(_nexus_runtime_scope OR NOT DEFINED NEXUS_SOC_FAMILY OR
        NEXUS_SOC_FAMILY STREQUAL "native")
    set(_nexus_io_domains gpio uart spi i2c flash watchdog exti pwm adc can
        usb_device eth eth_frames timer_capture advanced_pwm pwm_group encoder
        pwm_adc can_fd adc_group rtc lptim entropy)
endif()
foreach(domain IN LISTS _nexus_io_domains)
    if(NOT TARGET nexus_io_${domain})
        message(FATAL_ERROR "No I/O source contract for selected kind ${domain}")
    endif()
    target_link_libraries(nexus_io PUBLIC nexus_io_${domain})
endforeach()
add_library(Nexus::IO ALIAS nexus_io)
add_library(nexus_io_cycle_probe STATIC EXCLUDE_FROM_ALL
    src/mechanisms/cycle_probe.c)
target_link_libraries(nexus_io_cycle_probe PUBLIC nexus_io
    nexus_io_uart nexus_io_spi
    PRIVATE nexus_arch nexus_build_options nexus_owned_warnings)
add_library(Nexus::IOCycleProbe ALIAS nexus_io_cycle_probe)
