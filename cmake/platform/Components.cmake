# CMake owns the explicit component target map; authored input owns selection.
# Author: Nexus Team
include_guard(GLOBAL)

function(_nexus_component_bindings output)
    set(${output}
        "bus-owner=Nexus::BusOwner"
        "spi-owner=Nexus::SpiOwner"
        "spi-async-owner=Nexus::SPIAsyncOwner"
        "spi-dma-owner=Nexus::SPIDMATransactionOwner"
        "uart-owner=Nexus::UARTOwner"
        "owner-observer=Nexus::OwnerObserver"
        "bmp280=Nexus::BMP280"
        "bmp280-spi=Nexus::BMP280SPI"
        "log=Nexus::Log"
        "storage=Nexus::Storage"
        "settings=Nexus::Settings"
        "flash-storage=Nexus::FlashStorage"
        "modbus-rtu=Nexus::ModbusRTU"
        "network=Nexus::LwIP"
        "network-loaned-rx=Nexus::LwIPLoanedRX"
        "tls=Nexus::TLS"
        "mqtt=Nexus::CoreMQTT"
        "mcuboot=Nexus::MCUBootControl"
        "st-usb-host=Nexus::STUSBHost"
        "st-usb-device=Nexus::STUSBDevice"
        PARENT_SCOPE)
endfunction()

function(nexus_component_choices output)
    _nexus_component_bindings(bindings)
    set(choices)
    foreach(binding IN LISTS bindings)
        string(REPLACE "=" ";" pair "${binding}")
        list(GET pair 0 name)
        list(APPEND choices "${name}")
    endforeach()
    set(${output} "${choices}" PARENT_SCOPE)
endfunction()

function(nexus_resolve_components output)
    _nexus_component_bindings(bindings)
    set(selected)
    foreach(component IN LISTS ARGN)
        set(target "")
        foreach(binding IN LISTS bindings)
            string(REPLACE "=" ";" pair "${binding}")
            list(GET pair 0 name)
            if(name STREQUAL component)
                list(GET pair 1 target)
                break()
            endif()
        endforeach()
        if(NOT target)
            message(FATAL_ERROR "No owned target for component ${component}")
        endif()
        if(component MATCHES "^network")
            nexus_add_lwip()
        elseif(component STREQUAL "tls")
            nexus_add_tls()
        elseif(component STREQUAL "mqtt")
            nexus_add_coremqtt()
        elseif(component STREQUAL "mcuboot")
            nexus_add_mcuboot_control()
        elseif(component STREQUAL "st-usb-host")
            nexus_add_st_usb_host()
        elseif(component STREQUAL "st-usb-device")
            nexus_add_st_usb_device()
        endif()
        if(NOT TARGET "${target}")
            message(FATAL_ERROR
                "Selected component target is unavailable: ${component}")
        endif()
        list(APPEND selected "${target}")
    endforeach()
    set(${output} "${selected}" PARENT_SCOPE)
endfunction()
