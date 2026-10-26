# Real selected FDCAN assemblies; only external register/clock ports modeled.
foreach(g4_fdcan_mode loopback normal)
    if(g4_fdcan_mode STREQUAL normal)
        set(g4_fdcan_input_name g474-can-fd-normal)
        set(g4_fdcan_target nexus_g474_fdcan_normal_factory_test)
        set(g4_fdcan_normal 1)
    else()
        set(g4_fdcan_input_name g474-can-fd-loopback)
        set(g4_fdcan_target nexus_g474_fdcan_factory_test)
        set(g4_fdcan_normal 0)
    endif()
set(g4_fdcan_input
    "${NEXUS_SOURCE_DIR}/cmake/package/verification/io_evolution/${g4_fdcan_input_name}.toml")
set(g4_fdcan_bundle "${CMAKE_CURRENT_BINARY_DIR}/g474-fdcan-factory-${g4_fdcan_mode}")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${g4_fdcan_input}"
    --output "${g4_fdcan_bundle}" RESULT_VARIABLE g4_fdcan_generated
    OUTPUT_VARIABLE g4_fdcan_output ERROR_VARIABLE g4_fdcan_error)
if(NOT g4_fdcan_generated EQUAL 0)
    message(FATAL_ERROR
        "G474 control fixture rejected: ${g4_fdcan_output}${g4_fdcan_error}")
endif()
file(READ "${g4_fdcan_bundle}/input_paths.json" g4_fdcan_inputs)
string(JSON g4_fdcan_input_count LENGTH "${g4_fdcan_inputs}")
math(EXPR g4_fdcan_last_input "${g4_fdcan_input_count} - 1")
foreach(index RANGE 0 ${g4_fdcan_last_input})
    string(JSON g4_fdcan_input_name MEMBER "${g4_fdcan_inputs}" ${index})
    string(JSON g4_fdcan_input_path GET "${g4_fdcan_inputs}"
        "${g4_fdcan_input_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${g4_fdcan_input_path}")
endforeach()
nexus_google_test(${g4_fdcan_target}
    SOURCES g474_fdcan_factory_test.cpp
        "${g4_fdcan_bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/fdcan.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${g4_fdcan_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts"
        "${NEXUS_SOURCE_DIR}/io/private")
target_include_directories(${g4_fdcan_target} SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(${g4_fdcan_target})
target_compile_definitions(${g4_fdcan_target} PRIVATE STM32G474xx
    USE_RTOS=0 USE_HAL_FDCAN_REGISTER_CALLBACKS=0
    NX_G474_FDCAN_FACTORY_NORMAL=${g4_fdcan_normal}
    _init=nx_g474_fdcan_fixture_init _fini=nx_g474_fdcan_fixture_fini)
target_compile_options(${g4_fdcan_target} PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/g474_fdcan_factory_model.h")

nexus_stm32_official(${g4_fdcan_target} g4)
endforeach()
