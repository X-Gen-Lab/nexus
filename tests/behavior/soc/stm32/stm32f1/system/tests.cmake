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

target_sources(
    nexus_stm32f1_timebase_test
    PRIVATE "${NEXUS_SOURCE_DIR}/arch/arm/cortex_m/nx_arch_sleep.c"
)
target_include_directories(
    nexus_stm32f1_timebase_test
    PRIVATE "${NEXUS_SOURCE_DIR}/tests/behavior/arch/arm/cortex_m"
)
target_compile_definitions(
    nexus_stm32f1_timebase_test
    PRIVATE NEXUS_ARCH_SLEEP_MODEL=1
)
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
nexus_f1_system_contract(
    nexus_stm32f1_crystal72_test
    STM32F103xB
    NEXUS_CLOCK_SOURCE_HSE=1
    NEXUS_CLOCK_HSE_BYPASS=0
    NEXUS_CLOCK_APB1_HZ=36000000U
    NEXUS_CLOCK_APB2_HZ=72000000U
    NEXUS_CLOCK_APB1_TIMER_HZ=72000000U
    NEXUS_CLOCK_FLASH_WAIT_STATES=2U
)
nexus_f1_system_contract(nexus_stm32f1_connectivity_timebase_test STM32F105xC)

nexus_f1_system_contract(
    nexus_stm32f1_connectivity_pll72_test
    STM32F105xC
    NEXUS_CLOCK_SOURCE_HSE=1
    NEXUS_CLOCK_HSE_BYPASS=0
    NEXUS_CLOCK_APB1_HZ=36000000U
    NEXUS_CLOCK_APB2_HZ=72000000U
    NEXUS_CLOCK_APB1_TIMER_HZ=72000000U
    NEXUS_CLOCK_FLASH_WAIT_STATES=2U
    NEXUS_CLOCK_PLL48_HZ=48000000U
)

nexus_f1_system_contract(
    nexus_stm32f101_hse36_test
    STM32F101xB
    NEXUS_CLOCK_SOURCE_HSE=1
    NEXUS_CLOCK_HSE_BYPASS=0
    NEXUS_CLOCK_APB1_HZ=36000000U
    NEXUS_CLOCK_APB2_HZ=36000000U
    NEXUS_CLOCK_APB1_TIMER_HZ=36000000U
    NEXUS_CLOCK_FLASH_WAIT_STATES=1U
)
nexus_f1_system_contract(
    nexus_stm32f102_hse48_test
    STM32F102xB
    NEXUS_CLOCK_SOURCE_HSE=1
    NEXUS_CLOCK_HSE_BYPASS=0
    NEXUS_CLOCK_APB1_HZ=24000000U
    NEXUS_CLOCK_APB2_HZ=48000000U
    NEXUS_CLOCK_APB1_TIMER_HZ=48000000U
    NEXUS_CLOCK_FLASH_WAIT_STATES=1U
    NEXUS_CLOCK_PLL48_HZ=48000000U
)

include("${CMAKE_CURRENT_LIST_DIR}/handoff.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/fault.cmake")
nexus_f1_system_contract(nexus_stm32f103_stop_test STM32F103xB TEST_F1_STOP=1)
target_link_libraries(nexus_stm32f103_stop_test PRIVATE Nexus::ClockDomain)
target_sources(
    nexus_stm32f103_stop_test
    PRIVATE "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1/system/stop.c"
)
target_compile_definitions(
    nexus_stm32f103_stop_test
    PRIVATE NX_STM32F1_STOP_OWNER=1
)

get_target_property(f1_stop_config Nexus::Config NEXUS_CONFIG_DIR)
target_include_directories(
    nexus_stm32f103_stop_test
    PRIVATE "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1/include" "${f1_stop_config}"
)

nexus_f1_system_contract(nexus_stm32f103_standby_test STM32F103xB TEST_F1_STANDBY=1)
target_link_libraries(nexus_stm32f103_standby_test PRIVATE Nexus::ClockDomain)
target_sources(
    nexus_stm32f103_standby_test
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/standby_test.cpp"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1/system/standby.c"
)
target_compile_definitions(
    nexus_stm32f103_standby_test
    PRIVATE NX_STM32F1_STANDBY_OWNER=1
)
target_include_directories(
    nexus_stm32f103_standby_test
    PRIVATE "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f1/include" "${f1_stop_config}"
)
