# Actual Connectivity adapter and shared Native DWC register model.
# Author: Nexus Team
foreach(selector STM32F105xC STM32F107xC)
    set(target "stm32f1_connectivity_dwc_${selector}")
    set(f1 "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
    nexus_google_test(${target}
        SOURCES
            "${CMAKE_CURRENT_LIST_DIR}/connectivity_test.cpp"
            "${f1}/drivers/usb/connectivity_device.c"
            "${f1}/drivers/gpio/gpio.c"
        INCLUDES
            "${f1}"
            "${NEXUS_SOURCE_DIR}/soc/stm32/common/dwc_otg_device"
            "${NEXUS_SOURCE_DIR}/tests/support/native/drivers"
            "${NEXUS_SOURCE_DIR}/arch/include"
        LIBRARIES Nexus::NativeModel Nexus::Core Nexus::IOUSBDevice
    )
    nexus_use_cmsis_core(${target})
    target_include_directories(
        ${target}
        SYSTEM
        PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
            "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
    )
    target_compile_definitions(
        ${target}
        PRIVATE
            ${selector}
            NEXUS_CORE_HZ=72000000U
            NEXUS_CLOCK_PLL48_HZ=48000000U
    )
    target_compile_options(
        ${target}
        PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/connectivity_model.h"
    )
endforeach()

set(pma_target stm32f103_pma_device_contracts)
set(pma "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1/drivers/usb")
nexus_google_test(
    ${pma_target}
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/ep0_test.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/pma_test.cpp"
        "${pma}/pma.c"
        "${pma}/ep0.c"
        "${pma}/lp.c"
        "${pma}/link.c"
        "${pma}/pma_device.c"
        "${pma}/pma_ep1.c"
    INCLUDES "${pma}" "${NEXUS_SOURCE_DIR}/io/private"
    LIBRARIES Nexus::Arch Nexus::Core Nexus::IOUSBDevice Nexus::IOGPIO
)
nexus_use_cmsis_core(${pma_target})
target_include_directories(
    ${pma_target}
    SYSTEM
    PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
)
target_compile_definitions(
    ${pma_target}
    PRIVATE STM32F103xB NEXUS_CMSIS_HOST_MODEL=1 NX_STM32F1_USB_PMA_EP1=1
)

set(host_targets
    stm32f1_connectivity_host_contracts
    stm32f1_connectivity_bulk_host_contracts
)
if(TARGET Nexus::STUSBHost)
    list(APPEND host_targets stm32f1_connectivity_st_host_contracts)
endif()
set(host_shared "${NEXUS_SOURCE_DIR}/soc/stm32/common/dwc_otg_host")
foreach(host_target IN LISTS host_targets)
    nexus_google_test(
        ${host_target}
        SOURCES
            "${CMAKE_CURRENT_LIST_DIR}/connectivity_host_test.cpp"
            "${host_shared}/usb_host.c"
        INCLUDES
            "${f1}"
            "${host_shared}"
            "${f1}/sdk/hal_config/usb"
        LIBRARIES Nexus::Arch Nexus::Core Nexus::IOUSBHost Nexus::IODMA
    )
    nexus_use_cmsis_core(${host_target})
    target_include_directories(
        ${host_target}
        SYSTEM
        PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
            "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
    )
    target_compile_definitions(
        ${host_target}
        PRIVATE
            NX_STM32F1_USB_HOST=1
            NEXUS_CORE_HZ=72000000U
            NEXUS_CLOCK_PLL48_HZ=48000000U
    )
    target_compile_options(
        ${host_target}
        PRIVATE "-include${CMAKE_CURRENT_LIST_DIR}/connectivity_host_model.h"
    )
    if(host_target STREQUAL "stm32f1_connectivity_st_host_contracts")
        target_compile_definitions(
            ${host_target}
            PRIVATE STM32F107xC NX_STM32F1_USB_HOST_CORE=1
        )
        target_link_libraries(${host_target} PRIVATE Nexus::STUSBHost)
        target_include_directories(
            ${host_target}
            PRIVATE "${NEXUS_SOURCE_DIR}/components/protocols/st_usb/private"
        )
        target_include_directories(
            ${host_target}
            SYSTEM
            PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/usb_host/Core/Inc"
        )
    elseif(host_target STREQUAL "stm32f1_connectivity_bulk_host_contracts")
        target_compile_definitions(
            ${host_target}
            PRIVATE STM32F107xC NX_STM32F1_USB_HOST_BULK=1
        )
    else()
        target_compile_definitions(${host_target} PRIVATE STM32F105xC)
    endif()
endforeach()

# The components domain creates this locked, otherwise optional middleware.
if(TARGET Nexus::STUSBDevice)
    set(st_fixture
        "${NEXUS_SOURCE_DIR}/tests/integration/assemblies/stm32f1_pma_device"
    )
    target_sources(${pma_target} PRIVATE "${st_fixture}/st_bulk.c")
    target_compile_definitions(${pma_target} PRIVATE NX_TEST_PMA_ST_DEVICE=1)
    target_link_libraries(${pma_target} PRIVATE Nexus::STUSBDevice)
    target_include_directories(
        ${pma_target}
        PRIVATE
            "${st_fixture}"
            "${NEXUS_SOURCE_DIR}/components/protocols/st_usb_device/private"
    )
    target_include_directories(
        ${pma_target}
        SYSTEM
        PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/usb_device/Core/Inc"
    )
endif()
