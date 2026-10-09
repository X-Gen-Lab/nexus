# Host production-path MCU lifecycle contracts. Real HAL/core/runtime/platform
# code; explicit CPU, OSAL and vendor fault ports are models, never physical HIL.
if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    return() # Cortex vendor weak/register models are GNU/Clang-only fixtures.
endif()

# Keep instrumentation separate from Nexus::Config's generated header directory,
# configuration macros and CPU ABI; each model uses its isolated fake config.
foreach(chip stm32 gd32)
    foreach(backend baremetal freertos)
        set(target "mcu_lifecycle_${chip}_${backend}_tests")
        if(chip STREQUAL "stm32")
            set(hook "${NEXUS_SOURCE_DIR}/platforms/stm32/src/boot/stm32_platform_init.c")
        else()
            set(hook "${NEXUS_SOURCE_DIR}/platforms/gd32f470/src/platform.c")
        endif()
        add_executable(${target} test_platform.c "${hook}"
            "${NEXUS_SOURCE_DIR}/hal/src/nx_hal.c"
            "${NEXUS_SOURCE_DIR}/hal/src/nx_device.c"
            "${NEXUS_SOURCE_DIR}/runtime/src/nx_runtime.c")
        if(TARGET nexus_instrumentation_options)
            target_link_libraries(${target} PRIVATE nexus_instrumentation_options)
        endif()
        target_include_directories(${target} PRIVATE platform_model
            "${NEXUS_SOURCE_DIR}/hal/include" "${NEXUS_SOURCE_DIR}/arch/include"
            "${NEXUS_SOURCE_DIR}/osal/include" "${NEXUS_SOURCE_DIR}/runtime/include"
            "${NEXUS_SOURCE_DIR}/soc/gd32f470/include")
        target_compile_definitions(${target} PRIVATE NEXUS_PLATFORM_NATIVE=1)
        if(chip STREQUAL "stm32")
            target_compile_definitions(${target} PRIVATE STM32F407xx=1 NX_MODEL_STM32=1)
        endif()
        if(backend STREQUAL "baremetal")
            target_compile_definitions(${target} PRIVATE NX_CONFIG_OSAL_BAREMETAL=1)
        else()
            target_compile_definitions(${target} PRIVATE NX_CONFIG_OSAL_FREERTOS=1)
        endif()
        target_compile_features(${target} PRIVATE c_std_11)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -UNDEBUG)
        endif()
        foreach(scenario initial-resource-busy idle-reinit owner-busy osal-rollback cleanup-quarantine
            init-quarantine clock-cleanup-quarantine preflight-busy legacy-active context direct-hal-busy)
            add_test(NAME "${target}.${scenario}" COMMAND ${target} "${scenario}")
            set_tests_properties("${target}.${scenario}" PROPERTIES TIMEOUT 10
                LABELS "hal;runtime;${chip};${backend};host-model;lifecycle")
        endforeach()
        if(backend STREQUAL "freertos")
            add_test(NAME "${target}.kernel-running" COMMAND ${target} kernel-running)
            set_tests_properties("${target}.kernel-running" PROPERTIES TIMEOUT 10
                LABELS "hal;runtime;${chip};freertos;host-model;lifecycle")
        endif()
    endforeach()
endforeach()

# Execute the real SoC lifecycle helpers against observable register models.
# Ordinary vendor accesses are modeled; oscillator/switch/clock faults are
# injected at those accesses instead of replacing helper return values.
add_executable(mcu_register_stm32_tests test_registers_stm32.c
    register_model/stm32/model.c
    "${NEXUS_SOURCE_DIR}/soc/stm32f407/clock/stm32_clock_lifecycle.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32f407/interrupt/stm32_resources_lifecycle.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32f407/interrupt/stm32_isr_manager.c")
target_include_directories(mcu_register_stm32_tests PRIVATE
    register_model/stm32 register_model/common
    "${NEXUS_SOURCE_DIR}/soc/stm32f407/private"
    "${NEXUS_SOURCE_DIR}/hal/include" "${NEXUS_SOURCE_DIR}/arch/include")
target_compile_definitions(mcu_register_stm32_tests PRIVATE STM32F407xx=1)
add_executable(mcu_register_gd32_tests test_registers_gd32.c
    register_model/gd32/model.c "${NEXUS_SOURCE_DIR}/soc/gd32f470/clock.c"
    "${NEXUS_SOURCE_DIR}/soc/gd32f470/interrupt.c")
target_include_directories(mcu_register_gd32_tests PRIVATE
    register_model/gd32 register_model/common
    "${NEXUS_SOURCE_DIR}/soc/gd32f470/include"
    "${NEXUS_SOURCE_DIR}/hal/include" "${NEXUS_SOURCE_DIR}/arch/include")
foreach(chip stm32 gd32)
    if(TARGET nexus_instrumentation_options)
        target_link_libraries(mcu_register_${chip}_tests PRIVATE nexus_instrumentation_options)
    endif()
    target_compile_features(mcu_register_${chip}_tests PRIVATE c_std_11)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(mcu_register_${chip}_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    if(chip STREQUAL "stm32")
        set(register_cases clock clock_faults resources registration)
    else()
        set(register_cases clock clock_faults resources timebase timebase_faults)
    endif()
    foreach(scenario IN LISTS register_cases)
        add_test(NAME "mcu_register_${chip}_tests.${scenario}"
            COMMAND mcu_register_${chip}_tests "${scenario}")
        set_tests_properties("mcu_register_${chip}_tests.${scenario}" PROPERTIES
            TIMEOUT 10 LABELS "soc;${chip};host-register-model;lifecycle")
    endforeach()
endforeach()
