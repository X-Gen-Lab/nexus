# Execute real sixteen-pad factories, including nondefault raw resolutions.
foreach(adc_variant ve zg ve6 zg10)
    string(REGEX REPLACE "[0-9]+$" "" adc_density "${adc_variant}")
    set(adc_bits 12)
    set(adc_fixture "${NEXUS_SOURCE_DIR}/tests/contracts/adc_external_assembly")
    set(adc_input "${adc_fixture}/${adc_density}-scan.toml")
    if(adc_variant MATCHES "([0-9]+)$")
        set(adc_bits "${CMAKE_MATCH_1}")
        file(READ "${adc_input}" adc_assembly)
        string(REPLACE "board_package = \"${adc_density}_board\""
            "board_package = \"${adc_fixture}/${adc_density}_board\""
            adc_assembly "${adc_assembly}")
        string(APPEND adc_assembly "\nresolution_bits = ${adc_bits}\n")
        set(adc_input "${CMAKE_CURRENT_BINARY_DIR}/adc-factory-inputs/${adc_variant}.toml")
        file(CONFIGURE OUTPUT "${adc_input}" CONTENT "${adc_assembly}" @ONLY)
    endif()
    set(adc_bundle "${CMAKE_CURRENT_BINARY_DIR}/f407-adc-${adc_variant}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}"
        --assembly "${adc_input}"
        --output "${adc_bundle}" COMMAND_ERROR_IS_FATAL ANY)
    nexus_google_configuration_inputs("${adc_bundle}")
    set(adc_target "nexus_stm32_adc_${adc_variant}_cold_factory_test")
    nexus_google_test(${adc_target} STM32F4_LL
        SOURCES stm32_adc_cold_factory_test.cpp "${adc_bundle}/bindings.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/adc.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/adc_stream.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/adc_trigger.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_cold.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_lifecycle.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_coordinator.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/system_lifecycle.c"
        LIBRARIES Nexus::IO
        INCLUDES "${adc_bundle}"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
            "${NEXUS_SOURCE_DIR}/arch/include"
            "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu")
    target_include_directories(${adc_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    target_compile_definitions(${adc_target} PRIVATE STM32F407xx
        NX_TEST_ADC_BITS=${adc_bits}
        _init=nx_adc_cold_fixture_init _fini=nx_adc_cold_fixture_fini)
    target_compile_options(${adc_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_adc_cold_model.h")
endforeach()

# Internal analog contracts run once for the shared F407 implementation.
target_compile_definitions(nexus_stm32_adc_ve_cold_factory_test PRIVATE
    NX_ADC_INTERNAL_CONTRACT)
target_sources(nexus_stm32_adc_ve_cold_factory_test PRIVATE
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/adc_continuous.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/adc_injected.c")

# Pinless ADC selection must retain resource admission before System.start.
set(adc_internal_bundle "${CMAKE_CURRENT_BINARY_DIR}/f407-adc-internal")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}"
    --assembly "${NEXUS_SOURCE_DIR}/tests/contracts/multi_controller_assembly/stm32-ve-adc-internal-baremetal.toml"
    --output "${adc_internal_bundle}" COMMAND_ERROR_IS_FATAL ANY)
nexus_google_configuration_inputs("${adc_internal_bundle}")
nexus_google_test(nexus_stm32_adc_internal_cold_factory_test STM32F4_LL
    SOURCES stm32_adc_internal_factory_test.cpp
        "${adc_internal_bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/adc.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/adc_stream.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/adc_trigger.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_lifecycle.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_coordinator.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/system_lifecycle.c"
    LIBRARIES Nexus::IO
    INCLUDES "${adc_internal_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/arch/include"
        "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu")
target_include_directories(nexus_stm32_adc_internal_cold_factory_test
    SYSTEM PRIVATE "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
target_compile_definitions(nexus_stm32_adc_internal_cold_factory_test PRIVATE
    STM32F407xx _init=nx_adc_cold_fixture_init _fini=nx_adc_cold_fixture_fini)
target_compile_options(nexus_stm32_adc_internal_cold_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_adc_cold_model.h")
