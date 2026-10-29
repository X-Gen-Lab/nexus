# Actual polling factory: exact F407ZG fixture, concrete C owner and MMIO leaves.
set(f407_poll_bundle "${CMAKE_CURRENT_BINARY_DIR}/f407-i2c-poll-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly
    "${NEXUS_SOURCE_DIR}/tests/contracts/stm32_assembly/zg-i2c.toml"
    --output "${f407_poll_bundle}" RESULT_VARIABLE f407_poll_generated
    OUTPUT_VARIABLE f407_poll_output ERROR_VARIABLE f407_poll_error)
if(NOT f407_poll_generated EQUAL 0)
    message(FATAL_ERROR
        "F407 polling factory rejected: ${f407_poll_output}${f407_poll_error}")
endif()
foreach(f407_poll_navigation input_paths.json configuration_code_paths.json)
    file(READ "${f407_poll_bundle}/${f407_poll_navigation}" f407_poll_inputs)
    string(JSON f407_poll_count LENGTH "${f407_poll_inputs}")
    math(EXPR f407_poll_last "${f407_poll_count} - 1")
    foreach(index RANGE 0 ${f407_poll_last})
        string(JSON label MEMBER "${f407_poll_inputs}" ${index})
        string(JSON input GET "${f407_poll_inputs}" "${label}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input}")
    endforeach()
endforeach()
nexus_google_test(nexus_stm32_i2c_poll_factory_test STM32F4_LL
    SOURCES stm32_i2c_poll_factory_test.cpp "${f407_poll_bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/i2c.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/i2c_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/platform_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/gpio.c"
    LIBRARIES Nexus::IO
    INCLUDES "${f407_poll_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_stm32_i2c_poll_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
target_compile_definitions(nexus_stm32_i2c_poll_factory_test PRIVATE
    STM32F407xx _init=nx_i2c_poll_fixture_init _fini=nx_i2c_poll_fixture_fini)
target_compile_options(nexus_stm32_i2c_poll_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_platform_cold_model.h"
    "-include${CMAKE_CURRENT_SOURCE_DIR}/i2c_register_model.h")
