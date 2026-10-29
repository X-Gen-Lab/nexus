# Real selected control assembly; external register/clock ports alone modeled.
set(g4_control_input
    "${NEXUS_SOURCE_DIR}/tests/contracts/g474_control_assembly/pwm-group.toml")
set(g4_control_bundle "${CMAKE_CURRENT_BINARY_DIR}/g474-control-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${g4_control_input}"
    --output "${g4_control_bundle}" RESULT_VARIABLE g4_control_generated
    OUTPUT_VARIABLE g4_control_output ERROR_VARIABLE g4_control_error)
if(NOT g4_control_generated EQUAL 0)
    message(FATAL_ERROR
        "G474 control fixture rejected: ${g4_control_output}${g4_control_error}")
endif()
nexus_google_configuration_inputs("${g4_control_bundle}")
nexus_google_test(nexus_g474_control_factory_test
    SOURCES g474_control_factory_test.cpp
        "${g4_control_bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/platform_cold.c"
        "${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/pwm_group_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/pwm_group.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${g4_control_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts")
target_include_directories(nexus_g474_control_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_g474_control_factory_test)
target_compile_definitions(nexus_g474_control_factory_test PRIVATE STM32G474xx
    _init=nx_g474_control_fixture_init _fini=nx_g474_control_fixture_fini)
target_compile_options(nexus_g474_control_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/g474_control_factory_model.h")
