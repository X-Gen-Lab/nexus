# One real G474 assembly; only external CPU and MMIO leaves are modeled.
set(g4_factory_input "${NEXUS_SOURCE_DIR}/tests/contracts/g474_assembly/io.toml")
set(g4_factory_bundle "${CMAKE_CURRENT_BINARY_DIR}/g474-factory")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/configure/__init__.py"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/configure/bindings.py"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/configure/emission.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${g4_factory_input}"
    --output "${g4_factory_bundle}" RESULT_VARIABLE g4_generated
    OUTPUT_VARIABLE g4_output ERROR_VARIABLE g4_error)
if(NOT g4_generated EQUAL 0)
    message(FATAL_ERROR "G474 factory fixture rejected: ${g4_output}${g4_error}")
endif()
file(READ "${g4_factory_bundle}/input_paths.json" g4_inputs)
string(JSON g4_input_count LENGTH "${g4_inputs}")
math(EXPR g4_last_input "${g4_input_count} - 1")
foreach(index RANGE 0 ${g4_last_input})
    string(JSON g4_input_name MEMBER "${g4_inputs}" ${index})
    string(JSON g4_input_path GET "${g4_inputs}" "${g4_input_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${g4_input_path}")
endforeach()
nexus_google_test(nexus_g474_factory_test SOURCES g474_factory_test.cpp
    "${g4_factory_bundle}/bindings.c"
    "${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/gpio.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/uart.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${g4_factory_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts")
target_include_directories(nexus_g474_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_g474_factory_test)
target_compile_definitions(nexus_g474_factory_test PRIVATE STM32G474xx
    _init=nx_g474_fixture_init _fini=nx_g474_fixture_fini)
target_compile_options(nexus_g474_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/g474_factory_model.h")
