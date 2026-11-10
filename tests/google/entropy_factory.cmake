# Actual generated RNG selection, direct private transport and Platform calls.
set(entropy_bundle "${CMAKE_CURRENT_BINARY_DIR}/f407-entropy-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py" --test-support
    --source-root "${NEXUS_SOURCE_DIR}" --assembly
    "${NEXUS_SOURCE_DIR}/tests/contracts/entropy_assembly/ve.toml"
    --output "${entropy_bundle}" RESULT_VARIABLE entropy_generated
    OUTPUT_VARIABLE entropy_output ERROR_VARIABLE entropy_error)
if(NOT entropy_generated EQUAL 0)
    message(FATAL_ERROR "RNG factory rejected: ${entropy_output}${entropy_error}")
endif()
nexus_google_configuration_inputs("${entropy_bundle}")
nexus_google_test(nexus_entropy_factory_test STM32F4_LL
    SOURCES entropy_factory_test.cpp
        "${entropy_bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/entropy.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_lifecycle.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_coordinator.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/system_lifecycle.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio.c"
    LIBRARIES Nexus::IO
    INCLUDES "${entropy_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/include"
        "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_entropy_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
target_compile_definitions(nexus_entropy_factory_test PRIVATE STM32F407xx
    _init=nx_entropy_fixture_init _fini=nx_entropy_fixture_fini)
target_compile_options(nexus_entropy_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/entropy_factory_model.h")
