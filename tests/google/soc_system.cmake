
foreach(system_target nexus_stm32_system_transaction_test
                      nexus_stm32_system_84_transaction_test
                      nexus_stm32_system_hsi84_transaction_test)
nexus_google_test(${system_target}
    SOURCES stm32_system_transaction_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/power.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/supervision.c"
        "${NEXUS_SOURCE_DIR}/components/clock_domain/src/clock_domain.c"
    LIBRARIES Nexus::Core
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(${system_target} PRIVATE
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/include"
    "${NEXUS_SOURCE_DIR}/components/clock_domain/include"
        "${NEXUS_SOURCE_DIR}/components/clock_domain/private"
        "${NEXUS_SOURCE_DIR}/io/include")
target_include_directories(${system_target} SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(${system_target})
target_compile_definitions(${system_target} PRIVATE
    STM32F407xx NEXUS_STM32_MODEL)
target_compile_options(${system_target} PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_system_model.h")
if(system_target STREQUAL "nexus_stm32_system_84_transaction_test")
    target_compile_definitions(${system_target} PRIVATE
        NEXUS_SYSTEM_CLOCK_TEST_84=1)
endif()
if(system_target STREQUAL "nexus_stm32_system_hsi84_transaction_test")
    target_compile_definitions(${system_target} PRIVATE
        NEXUS_SYSTEM_CLOCK_TEST_84=1 NEXUS_SYSTEM_CLOCK_TEST_HSI84=1)
endif()
endforeach()

nexus_google_test(nexus_stm32_power_rtc_integration_test STM32F4_LL
    SOURCES power_rtc_integration_test.cpp f407_rtc_model.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/power.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/rtc.c"
        "${NEXUS_SOURCE_DIR}/components/clock_domain/src/clock_domain.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/include"
        "${NEXUS_SOURCE_DIR}/components/clock_domain/include"
        "${NEXUS_SOURCE_DIR}/components/clock_domain/private"
        "${NEXUS_SOURCE_DIR}/arch/include"
        "${NEXUS_SOURCE_DIR}/io/private")
target_include_directories(nexus_stm32_power_rtc_integration_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_stm32_power_rtc_integration_test)
target_compile_definitions(nexus_stm32_power_rtc_integration_test PRIVATE
    STM32F407xx NEXUS_STM32_MODEL NX_F407_RTC_REAL_SYSTEM
    nx_arch_is_privileged=nx_f407_rtc_privileged
    nx_arch_in_isr=nx_f407_rtc_in_isr
    nx_arch_irq_is_masked=nx_f407_rtc_masked
    nx_arch_irq_masks=nx_f407_rtc_masks
    nx_arch_dsb=nx_f407_rtc_dsb)
target_compile_options(nexus_stm32_power_rtc_integration_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/power_rtc_integration_model.h")
foreach(cpu_leaf irq_save irq_restore irq_is_masked irq_masks is_privileged
                 in_isr dsb isb)
    target_link_options(nexus_stm32_power_rtc_integration_test PRIVATE
        "LINKER:--wrap=nx_arch_${cpu_leaf}")
endforeach()

foreach(input_profile hsi hse hsi_pll48 hse_pll48)
    set(input_target "nexus_stm32_input_${input_profile}_clock_test")
    nexus_google_test(${input_target}
        SOURCES stm32_input_clock_test.cpp
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/system.c"
        LIBRARIES Nexus::Core
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
            "${NEXUS_SOURCE_DIR}/arch/include")
    target_include_directories(${input_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    nexus_use_cmsis_core(${input_target})
    if(input_profile MATCHES "^hse")
        set(input_hse 1)
    else()
        set(input_hse 0)
    endif()
    if(input_profile MATCHES "pll48$")
        set(input_pll48 1)
    else()
        set(input_pll48 0)
    endif()
    target_compile_definitions(${input_target} PRIVATE STM32F407xx
        NEXUS_STM32_MODEL NEXUS_INPUT_HSE=${input_hse}
        NEXUS_INPUT_PLL48=${input_pll48})
    target_compile_options(${input_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_input_clock_model.h")
endforeach()
