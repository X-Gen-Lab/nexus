# F1 time runs independently of the kernel and retains failed receipts.
# Author: Nexus Team
function(nexus_f1_system_contract target device)
    set(system_package "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1")
    nexus_google_test(${target}
        SOURCES "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/timebase_test.cpp"
            "${system_package}/system/platform.c"
        LIBRARIES Nexus::Arch Nexus::Core Nexus::IOWatchdog
        INCLUDES "${system_package}"
    )
    target_include_directories(
        ${target}
        SYSTEM
        PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
    )
    nexus_use_cmsis_core(${target})
    target_compile_definitions(${target} PRIVATE ${device} ${ARGN})
    target_compile_options(
        ${target}
        PRIVATE "-include${CMAKE_CURRENT_FUNCTION_LIST_DIR}/register_model.h"
    )
endfunction()

nexus_f1_system_contract(nexus_stm32f1_timebase_test STM32F103xB)
nexus_f1_system_contract(
    nexus_stm32f1_hse72_test
    STM32F103xB
    NEXUS_CLOCK_SOURCE_HSE=1
    NEXUS_CLOCK_HSE_BYPASS=1
    NEXUS_CLOCK_APB1_HZ=36000000U
    NEXUS_CLOCK_APB2_HZ=72000000U
    NEXUS_CLOCK_APB1_TIMER_HZ=72000000U
    NEXUS_CLOCK_FLASH_WAIT_STATES=2U
)
nexus_f1_system_contract(nexus_stm32f1_connectivity_timebase_test STM32F105xC)
