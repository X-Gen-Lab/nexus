# Exact generated single-shot ADC and fixed PWM lifecycle with real providers.
foreach(control_kind adc pwm exti)
    set(control_bundle "${CMAKE_CURRENT_BINARY_DIR}/f407-control-${control_kind}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}" --assembly
        "${NEXUS_SOURCE_DIR}/tests/contracts/stm32_assembly/zg-${control_kind}.toml"
        --output "${control_bundle}" RESULT_VARIABLE control_generated
        OUTPUT_VARIABLE control_output ERROR_VARIABLE control_error)
    if(NOT control_generated EQUAL 0)
        message(FATAL_ERROR "F407 control factory rejected: ${control_output}${control_error}")
    endif()
    nexus_google_configuration_inputs("${control_bundle}")
    if(control_kind STREQUAL "pwm")
        set(control_driver timer)
    else()
        set(control_driver "${control_kind}")
    endif()
    set(control_target "nexus_stm32_${control_kind}_cold_factory_test")
    nexus_google_test(${control_target} STM32F4_LL
        SOURCES stm32_control_factory_test.cpp "${control_bundle}/bindings.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/${control_driver}.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/gpio.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/platform_cold.c"
        LIBRARIES Nexus::IO
        INCLUDES "${control_bundle}"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model"
            "${NEXUS_SOURCE_DIR}/arch/include")
    target_include_directories(${control_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    target_compile_definitions(${control_target} PRIVATE STM32F407xx
        _init=nx_control_fixture_init _fini=nx_control_fixture_fini)
    if(control_kind STREQUAL "pwm")
        target_compile_definitions(${control_target} PRIVATE NX_CONTROL_PWM)
    elseif(control_kind STREQUAL "exti")
        target_compile_definitions(${control_target} PRIVATE NX_CONTROL_EXTI)
    endif()
    target_compile_options(${control_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_control_cold_model.h")
endforeach()
