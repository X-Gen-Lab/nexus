# Actual normal TOML bundle; model only external register and time behavior.
set(g4_pwm_adc_input
    "${NEXUS_SOURCE_DIR}/cmake/package/verification/io_evolution/g474-pwm-adc.toml")
set(g4_pwm_adc_bundle "${CMAKE_CURRENT_BINARY_DIR}/g474-pwm-adc-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${g4_pwm_adc_input}"
    --output "${g4_pwm_adc_bundle}" RESULT_VARIABLE g4_pwm_adc_generated
    OUTPUT_VARIABLE g4_pwm_adc_output ERROR_VARIABLE g4_pwm_adc_error)
if(NOT g4_pwm_adc_generated EQUAL 0)
    message(FATAL_ERROR
        "G474 joint fixture rejected: ${g4_pwm_adc_output}${g4_pwm_adc_error}")
endif()
file(READ "${g4_pwm_adc_bundle}/input_paths.json" g4_pwm_adc_inputs)
string(JSON g4_pwm_adc_input_count LENGTH "${g4_pwm_adc_inputs}")
math(EXPR g4_pwm_adc_last_input "${g4_pwm_adc_input_count} - 1")
foreach(index RANGE 0 ${g4_pwm_adc_last_input})
    string(JSON g4_pwm_adc_input_name MEMBER "${g4_pwm_adc_inputs}" ${index})
    string(JSON g4_pwm_adc_input_path GET "${g4_pwm_adc_inputs}"
        "${g4_pwm_adc_input_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${g4_pwm_adc_input_path}")
endforeach()
nexus_google_test(nexus_g474_pwm_adc_factory_test
    SOURCES g474_pwm_adc_factory_test.cpp g474_pwm_adc_factory_ports.cpp
        g474_pwm_adc_model.cpp "${g4_pwm_adc_bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/adc.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/pwm_group.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/pwm_adc.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${g4_pwm_adc_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/private"
        "${NEXUS_SOURCE_DIR}/io/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts")
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
nexus_stm32_official(nexus_g474_pwm_adc_factory_test g4)
target_include_directories(nexus_g474_pwm_adc_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_g474_pwm_adc_factory_test PRIVATE STM32G474xx
    NX_G474_PWM_ADC_EXTERNAL_CORE
    _init=nx_g474_pwm_adc_fixture_init _fini=nx_g474_pwm_adc_fixture_fini)
target_compile_options(nexus_g474_pwm_adc_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/g474_pwm_adc_factory_model.h")
