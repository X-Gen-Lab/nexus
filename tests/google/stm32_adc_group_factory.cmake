# Real generated factories exercise production NVIC/pad/Platform code paths.
foreach(group_variant ve zg)
    if(group_variant STREQUAL "ve")
        set(group_input ve-dual-regular.toml)
    else()
        set(group_input ve-dual-injected.toml)
    endif()
    set(group_bundle "${CMAKE_CURRENT_BINARY_DIR}/adc-group-${group_variant}")
    set(group_fixture "${NEXUS_SOURCE_DIR}/tests/contracts/adc_group_assembly")
    file(READ "${group_fixture}/${group_input}" group_text)
    string(REPLACE "board_package = \"ve_board\""
        "board_package = \"${group_fixture}/${group_variant}_board\""
        group_text "${group_text}")
    set(group_input "${CMAKE_CURRENT_BINARY_DIR}/adc-group-${group_variant}.toml")
    file(CONFIGURE OUTPUT "${group_input}" CONTENT "${group_text}" @ONLY)
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${group_input}"
        --output "${group_bundle}" COMMAND_ERROR_IS_FATAL ANY)
    nexus_google_configuration_inputs("${group_bundle}")
    set(group_target "nexus_stm32_adc_group_${group_variant}_factory_test")
    nexus_google_test(${group_target} STM32F4_LL
        SOURCES stm32_adc_group_factory_test.cpp "${group_bundle}/bindings.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/adc_group.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/adc_trigger.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_cold.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_lifecycle.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_coordinator.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/system_lifecycle.c"
        LIBRARIES Nexus::IO
        INCLUDES "${group_bundle}"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
            "${NEXUS_SOURCE_DIR}/arch/include"
            "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu")
    target_include_directories(${group_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    target_compile_definitions(${group_target} PRIVATE STM32F407xx
        _init=nx_adc_group_fixture_init _fini=nx_adc_group_fixture_fini)
    target_compile_options(${group_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_adc_group_factory_model.h")
    if(group_variant STREQUAL "ve")
        target_compile_definitions(${group_target} PRIVATE NX_ADC_GROUP_FACTORY_DMA)
    endif()
endforeach()
