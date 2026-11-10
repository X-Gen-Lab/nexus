# Only external register, reset, NVIC and CPU leaves are replaced by the model.
nexus_google_test(nexus_stm32_i2c_target_cold_test STM32F4_LL
    SOURCES i2c_target_cold_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/i2c_target.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio.c"
    LIBRARIES Nexus::Core
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/io/include"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_stm32_i2c_target_cold_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_stm32_i2c_target_cold_test)
target_compile_definitions(nexus_stm32_i2c_target_cold_test PRIVATE
    STM32F407xx NEXUS_CMSIS_HOST_MODEL=1
    nx_arch_is_privileged=nx_target_privileged
    nx_arch_in_isr=nx_target_in_isr
    nx_arch_irq_is_masked=nx_target_masked nx_arch_dsb=nx_target_dsb)
target_compile_options(nexus_stm32_i2c_target_cold_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/i2c_target_model.h")

set(target_bundle "${CMAKE_CURRENT_BINARY_DIR}/f407-i2c-target-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py" --test-support
    --source-root "${NEXUS_SOURCE_DIR}" --assembly
    "${NEXUS_SOURCE_DIR}/tests/contracts/i2c_target/ve-target.toml"
    --output "${target_bundle}" RESULT_VARIABLE target_generated
    OUTPUT_VARIABLE target_output ERROR_VARIABLE target_error)
if(NOT target_generated EQUAL 0)
    message(FATAL_ERROR
        "I2C target factory rejected: ${target_output}${target_error}")
endif()
foreach(navigation input_paths.json configuration_code_paths.json)
    file(READ "${target_bundle}/${navigation}" target_inputs)
    string(JSON count LENGTH "${target_inputs}")
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
        string(JSON label MEMBER "${target_inputs}" ${index})
        string(JSON input GET "${target_inputs}" "${label}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input}")
    endforeach()
endforeach()
nexus_google_test(nexus_stm32_i2c_target_factory_test STM32F4_LL
    SOURCES i2c_target_factory_test.cpp "${target_bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/i2c_target.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_lifecycle.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_coordinator.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/system_lifecycle.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio.c"
    LIBRARIES Nexus::IO
    INCLUDES "${target_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_stm32_i2c_target_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
target_compile_definitions(nexus_stm32_i2c_target_factory_test PRIVATE
    STM32F407xx _init=nx_target_factory_init _fini=nx_target_factory_fini)
target_compile_options(nexus_stm32_i2c_target_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/i2c_target_factory_model.h")

include("${CMAKE_CURRENT_LIST_DIR}/i2c_target_exchange.cmake")
