# Real exact-line G474 EXTI and generated factory; only MMIO/CPU leaves modeled.
set(g4_exti_bundle "${CMAKE_CURRENT_BINARY_DIR}/g474-exti-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}"
    --assembly "${NEXUS_SOURCE_DIR}/tests/contracts/g474_assembly/exti.toml"
    --output "${g4_exti_bundle}" RESULT_VARIABLE g4_exti_generated
    OUTPUT_VARIABLE g4_exti_output ERROR_VARIABLE g4_exti_error)
if(NOT g4_exti_generated EQUAL 0)
    message(FATAL_ERROR "G474 EXTI fixture rejected: ${g4_exti_output}${g4_exti_error}")
endif()
file(READ "${g4_exti_bundle}/input_paths.json" g4_exti_inputs)
string(JSON g4_exti_count LENGTH "${g4_exti_inputs}")
math(EXPR g4_exti_last "${g4_exti_count} - 1")
foreach(index RANGE 0 ${g4_exti_last})
    string(JSON g4_exti_name MEMBER "${g4_exti_inputs}" ${index})
    string(JSON g4_exti_path GET "${g4_exti_inputs}" "${g4_exti_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${g4_exti_path}")
endforeach()
nexus_google_test(nexus_g474_exti_test SOURCES g474_exti_test.cpp
    "${g4_exti_bundle}/bindings.c"
    "${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/exti.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${g4_exti_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts")
target_include_directories(nexus_g474_exti_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_g474_exti_test PRIVATE STM32G474xx
    _init=nx_g474_exti_fixture_init _fini=nx_g474_exti_fixture_fini)
target_compile_options(nexus_g474_exti_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/g474_exti_model.h")
