
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

# Preserve the distinguishing non-unit HPRE fault paths without repeating the
# complete system/RTC/power suite for another nominal clock spelling.
foreach(ahb_div 2 4)
    set(ahb_target "nexus_stm32_ahb${ahb_div}_clock_test")
    nexus_google_test(${ahb_target}
        SOURCES stm32_ahb_clock_test.cpp
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/system.c"
        LIBRARIES Nexus::Core
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
            "${NEXUS_SOURCE_DIR}/arch/include")
    target_include_directories(${ahb_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    nexus_use_cmsis_core(${ahb_target})
    target_compile_definitions(${ahb_target} PRIVATE STM32F407xx
        NEXUS_STM32_MODEL NEXUS_STM32_AHB_DIV=${ahb_div})
    target_compile_options(${ahb_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_ahb_clock_model.h")
endforeach()

# Compare the closed 75 MHz PLLQ context with the ordinary 48 MHz domain.
foreach(sdio_high_speed 0 1)
    set(sdio_system_target "nexus_stm32_sdio_system_${sdio_high_speed}_test")
    nexus_google_test(${sdio_system_target}
        SOURCES stm32_sdio_system_test.cpp
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/system.c"
        LIBRARIES Nexus::Core
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/include"
            "${NEXUS_SOURCE_DIR}/components/clock_domain/include"
            "${NEXUS_SOURCE_DIR}/components/clock_domain/private"
            "${NEXUS_SOURCE_DIR}/arch/include"
            "${NEXUS_SOURCE_DIR}/io/include")
    if(sdio_high_speed)
        target_sources(${sdio_system_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/power.c"
            "${NEXUS_SOURCE_DIR}/components/clock_domain/src/clock_domain.c")
    endif()
    target_include_directories(${sdio_system_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    nexus_use_cmsis_core(${sdio_system_target})
    target_compile_definitions(${sdio_system_target} PRIVATE
        STM32F407xx NEXUS_STM32_MODEL NX_TEST_SDIO_HS375=${sdio_high_speed})
    target_compile_options(${sdio_system_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_sdio_system_model.h")
endforeach()

nexus_google_test(nexus_stm32_silicon_identity_test
    SOURCES stm32_silicon_identity_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/system.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
target_include_directories(nexus_stm32_silicon_identity_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_stm32_silicon_identity_test)
target_compile_definitions(nexus_stm32_silicon_identity_test PRIVATE
    STM32F407xx NEXUS_STM32_MODEL)
target_compile_options(nexus_stm32_silicon_identity_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_silicon_identity_model.h")

foreach(power_rtc_part f407 f411 f446 f479)
    if(power_rtc_part STREQUAL "f407")
        set(power_rtc_target nexus_stm32_power_rtc_integration_test)
        set(power_rtc_macro STM32F407xx)
    elseif(power_rtc_part STREQUAL "f411")
        set(power_rtc_target nexus_stm32_f411_power_rtc_integration_test)
        set(power_rtc_macro STM32F411xE)
    elseif(power_rtc_part STREQUAL "f446")
        set(power_rtc_target nexus_stm32_f446_power_rtc_integration_test)
        set(power_rtc_macro STM32F446xx)
    else()
        set(power_rtc_target nexus_stm32_f479_power_rtc_integration_test)
        set(power_rtc_macro STM32F479xx)
    endif()
    nexus_google_test(${power_rtc_target} STM32F4_LL
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
    target_include_directories(${power_rtc_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    nexus_use_cmsis_core(${power_rtc_target})
    target_compile_definitions(${power_rtc_target} PRIVATE
        ${power_rtc_macro} NEXUS_STM32_MODEL NX_F407_RTC_REAL_SYSTEM
        nx_arch_is_privileged=nx_f407_rtc_privileged
        nx_arch_in_isr=nx_f407_rtc_in_isr
        nx_arch_irq_is_masked=nx_f407_rtc_masked
        nx_arch_irq_masks=nx_f407_rtc_masks
        nx_arch_dsb=nx_f407_rtc_dsb)
    target_compile_options(${power_rtc_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/power_rtc_integration_model.h")
    foreach(cpu_leaf irq_save irq_restore irq_is_masked irq_masks is_privileged
                     in_isr dsb isb)
        target_link_options(${power_rtc_target} PRIVATE
            "LINKER:--wrap=nx_arch_${cpu_leaf}")
    endforeach()

    if(NOT power_rtc_part STREQUAL "f407")
        target_compile_definitions(${power_rtc_target} PRIVATE
            NX_POWER_RTC_HIGH_PROFILE=1)
    endif()
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

foreach(family_profile f407 f411 f446 f479)
    if(family_profile STREQUAL "f407")
        set(family_macro STM32F407xx)
        set(family_device 0x413U)
        set(family_cut 0x1001U)
        set(family_kib 1024U)
    elseif(family_profile STREQUAL "f411")
        set(family_macro STM32F411xE)
        set(family_device 0x431U)
        set(family_cut 0x1000U)
        set(family_kib 512U)
    elseif(family_profile STREQUAL "f446")
        set(family_macro STM32F446xx)
        set(family_device 0x421U)
        set(family_cut 0x1000U)
        set(family_kib 512U)
    else()
        set(family_macro STM32F479xx)
        set(family_device 0x434U)
        set(family_cut 0x1000U)
        set(family_kib 2048U)
    endif()
    set(family_target "nexus_stm32_${family_profile}_system_test")
    nexus_google_test(${family_target}
        SOURCES stm32_family_system_test.cpp
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/system.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
    target_include_directories(${family_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    nexus_use_cmsis_core(${family_target})
    target_compile_definitions(${family_target} PRIVATE ${family_macro}
        NEXUS_STM32_MODEL NX_TEST_DEVICE=${family_device}
        NX_TEST_CUT=${family_cut} NX_TEST_FLASH_KIB=${family_kib})
    if(family_profile STREQUAL "f407")
        target_compile_definitions(${family_target} PRIVATE NX_TEST_SYSTEM_TIM5=1)
    endif()
    target_compile_options(${family_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_family_system_model.h")
    if(NOT family_profile STREQUAL "f407")
        set(high_target "nexus_stm32_${family_profile}_high_clock_test")
        nexus_google_test(${high_target}
            SOURCES stm32_high_clock_test.cpp
                "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/system.c"
                "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/power.c"
                "${NEXUS_SOURCE_DIR}/components/clock_domain/src/clock_domain.c"
            LIBRARIES Nexus::IO Nexus::Arch
            INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
                "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/include"
                "${NEXUS_SOURCE_DIR}/components/clock_domain/include"
                "${NEXUS_SOURCE_DIR}/components/clock_domain/private")
        target_include_directories(${high_target} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
        nexus_use_cmsis_core(${high_target})
        target_compile_definitions(${high_target} PRIVATE ${family_macro}
            NEXUS_STM32_MODEL NX_TEST_DEVICE=${family_device}
            NX_TEST_CUT=${family_cut} NX_TEST_FLASH_KIB=${family_kib})
        target_compile_options(${high_target} PRIVATE
            "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_high_clock_model.h")
    endif()
endforeach()


# One complete aux suite per distinct M layout; R-only / Q+R retain only their
# output-specific contract slice through NX_TEST_R_OUTPUT at compile time.
foreach(aux_case f446_q f479_q f479_r f479_qr)
    if(aux_case STREQUAL "f446_q")
        set(aux_device STM32F446xx)
    else()
        set(aux_device STM32F479xx)
    endif()
    if(aux_case STREQUAL "f479_r")
        set(aux_kind 2)
    elseif(aux_case STREQUAL "f479_qr")
        set(aux_kind 3)
    else()
        set(aux_kind 1)
    endif()
    set(aux_target "nexus_stm32_system_aux_${aux_case}_test")
    nexus_google_test(${aux_target}
        SOURCES stm32_sai_system_test.cpp
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/system.c"
        LIBRARIES Nexus::Core
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
            "${NEXUS_SOURCE_DIR}/arch/include")
    target_include_directories(${aux_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    nexus_use_cmsis_core(${aux_target})
    target_compile_definitions(${aux_target} PRIVATE
        ${aux_device} NEXUS_STM32_MODEL NX_TEST_AUX_KIND=${aux_kind}U)
    if(NOT aux_kind EQUAL 1)
        target_compile_definitions(${aux_target} PRIVATE NX_TEST_R_OUTPUT=1)
    endif()
    target_compile_options(${aux_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_sai_system_model.h")
endforeach()
