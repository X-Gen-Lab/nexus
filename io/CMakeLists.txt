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
    SOURCES src/api/spi.c src/api/spi_frames.c src/mechanisms/spi_dma_owner_binding.c
        src/mechanisms/spi_dma_sequence.c src/mechanisms/spi_dma_transaction.c
        src/mechanisms/spi_transaction.c
    DEPENDS nexus_io_stream)
nexus_io_domain(i2c ALIAS IOI2C
    SOURCES src/api/i2c.c src/mechanisms/i2c_bus_clear.c
    DEPENDS nexus_io_dma nexus_io_wake)
nexus_io_domain(i2c_target ALIAS IOI2CTarget SOURCES src/api/i2c_target.c)
nexus_io_domain(flash ALIAS IOFlash
    SOURCES src/api/flash.c src/api/flash_maintenance.c
        src/mechanisms/flash_region.c)
nexus_io_domain(flash_async ALIAS IOFlashAsync SOURCES src/api/flash_async.c
    DEPENDS nexus_io_flash)
nexus_io_domain(nand ALIAS IONAND SOURCES src/api/nand.c)
nexus_io_domain(block ALIAS IOBlock SOURCES src/api/block.c src/api/block_async.c
    src/api/sdio_io.c DEPENDS nexus_io_dma)
nexus_io_domain(external_bus ALIAS IOExternalBus SOURCES src/api/external_bus.c)
nexus_io_domain(parallel16 ALIAS IOParallel16 SOURCES src/api/parallel16.c)
nexus_io_domain(memory_window ALIAS IOMemoryWindow
    SOURCES src/api/memory_window.c)
nexus_io_domain(watchdog ALIAS IOWatchdog SOURCES src/api/watchdog.c)
nexus_io_domain(exti ALIAS IOEXTI SOURCES src/api/exti.c DEPENDS nexus_io_wake)
nexus_io_domain(pwm ALIAS IOTimer SOURCES src/api/timer.c)
nexus_io_domain(dac ALIAS IODAC SOURCES src/api/dac.c)
nexus_io_domain(crc ALIAS IOCRC SOURCES src/api/crc.c)
nexus_io_domain(cipher ALIAS IOCipher SOURCES src/api/cipher.c
    DEPENDS nexus_io_wake)
nexus_io_domain(hash ALIAS IOHash SOURCES src/api/hash.c
    DEPENDS nexus_io_wake)
nexus_io_domain(display ALIAS IODisplay SOURCES src/api/display.c
    DEPENDS nexus_io_dma nexus_io_wake)
nexus_io_domain(image ALIAS IOImage SOURCES src/api/image.c
    DEPENDS nexus_io_dma nexus_io_wake)
nexus_io_domain(dac_tx ALIAS IODACTx SOURCES src/api/dac_tx.c
    DEPENDS nexus_io_dma nexus_io_wake)
nexus_io_domain(adc ALIAS IOADC
    SOURCES src/api/adc.c src/mechanisms/adc_observer.c
        src/mechanisms/adc_stream_profile.c
    DEPENDS nexus_io_stream_profile_storage nexus_io_adc_format)
nexus_io_domain(can ALIAS IOCAN SOURCES src/api/can.c DEPENDS nexus_io_wake)
nexus_io_domain(usb_device ALIAS IOUSBDevice
    SOURCES src/api/usb_device.c src/mechanisms/usb_profile.c
    DEPENDS nexus_io_dma nexus_io_wake)
nexus_io_domain(usb_host ALIAS IOUSBHost SOURCES src/api/usb_host.c
    DEPENDS nexus_io_dma nexus_io_wake)
nexus_io_domain(usb_otg ALIAS IOUSBOTG SOURCES src/api/usb_otg.c
    DEPENDS nexus_io_usb_device nexus_io_usb_host)
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
nexus_io_domain(audio ALIAS IOAudio SOURCES src/api/audio.c
    DEPENDS nexus_io_stream)
nexus_io_domain(capture ALIAS IOCapture SOURCES src/api/capture.c
    DEPENDS nexus_io_stream)
nexus_io_domain(timer_pulse ALIAS IOTimerPulse SOURCES src/api/timer_pulse.c
    DEPENDS nexus_io_stream)
nexus_io_domain(timer_sequence ALIAS IOTimerSequence SOURCES src/api/timer_sequence.c
    DEPENDS nexus_io_stream)

# Common observation is usable without a physical controller. Every other
# source is compiled only through a selected domain or explicit consumer target.
add_library(nexus_io STATIC
    src/mechanisms/diagnostic.c src/mechanisms/observation.c)
target_link_libraries(nexus_io PUBLIC nexus_io_contract
    PRIVATE nexus_build_options nexus_owned_warnings Nexus::Arch)
set(_nexus_io_domains ${NEXUS_SELECTED_KINDS})
if("dac" IN_LIST _nexus_io_domains)
    include("${NEXUS_SOURCE_DIR}/cmake/platform/ControllerModes.cmake")
    nexus_controller_modes(dac_modes dac)
    if("trigger-dma" IN_LIST dac_modes OR "trigger-dma-circular" IN_LIST dac_modes)
        list(APPEND _nexus_io_domains dac_tx)
    endif()
endif()
# Runtime and Native admit caller-defined providers, independently of Board facts.
if(_nexus_runtime_scope OR NOT DEFINED NEXUS_SOC_FAMILY OR
        NEXUS_SOC_FAMILY STREQUAL "native")
    set(_nexus_io_domains gpio uart spi i2c i2c_target flash watchdog exti pwm adc dac dac_tx crc cipher hash display image can
        usb_device usb_host eth eth_frames timer_capture advanced_pwm pwm_group encoder
        pwm_adc can_fd adc_group rtc lptim entropy block external_bus
        parallel16 memory_window flash_async nand capture audio
        timer_pulse timer_sequence)
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
