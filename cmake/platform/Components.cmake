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
        "flash-storage=Nexus::FlashStorage"
        "modbus-rtu=Nexus::ModbusRTU"
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
        if(NOT TARGET "${target}")
            message(FATAL_ERROR
                "Selected component target is unavailable: ${component}")
        endif()
        list(APPEND selected "${target}")
    endforeach()
    set(${output} "${selected}" PARENT_SCOPE)
endfunction()
