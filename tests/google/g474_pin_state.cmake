# Selected GPIO states: direct private provider and actual static factory.
nexus_google_test(nexus_pin_state_test SOURCES pin_state_test.cpp
    LIBRARIES Nexus::IO)
nexus_google_test(nexus_g474_pin_state_test
    SOURCES g474_pin_state_test.cpp g474_pin_state_model.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/pin_state.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/io/private"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/private")
target_include_directories(nexus_g474_pin_state_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_g474_pin_state_test PRIVATE STM32G474xx)
target_compile_options(nexus_g474_pin_state_test PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-DNX_PIN_STATE_MODEL_C>"
    "$<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/g474_pin_state_model.h>")

set(g4_pins_input "${NEXUS_SOURCE_DIR}/tools/configure/assemblies/g474-pin-states.toml")
set(g4_pins_bundle "${CMAKE_CURRENT_BINARY_DIR}/g474-pin-states-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${g4_pins_input}"
    --output "${g4_pins_bundle}" RESULT_VARIABLE g4_pins_generated
    OUTPUT_VARIABLE g4_pins_output ERROR_VARIABLE g4_pins_error)
if(NOT g4_pins_generated EQUAL 0)
    message(FATAL_ERROR "G474 pin-state factory rejected: ${g4_pins_output}${g4_pins_error}")
endif()
foreach(g4_pins_navigation input_paths.json configuration_code_paths.json)
    file(READ "${g4_pins_bundle}/${g4_pins_navigation}" g4_pins_inputs)
    string(JSON g4_pins_count LENGTH "${g4_pins_inputs}")
    math(EXPR g4_pins_last "${g4_pins_count} - 1")
    foreach(index RANGE 0 ${g4_pins_last})
        string(JSON g4_pins_name MEMBER "${g4_pins_inputs}" ${index})
        string(JSON g4_pins_path GET "${g4_pins_inputs}" "${g4_pins_name}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${g4_pins_path}")
    endforeach()
endforeach()
nexus_google_test(nexus_g474_pin_state_factory_test
    SOURCES g474_pin_state_factory_test.cpp g474_pin_state_model.cpp
        "${g4_pins_bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/pin_state.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${g4_pins_bundle}"
        "${NEXUS_SOURCE_DIR}/io/private"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts")
target_include_directories(nexus_g474_pin_state_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_g474_pin_state_factory_test PRIVATE STM32G474xx
    NX_PIN_STATE_FACTORY_MODEL=1
    _init=nx_g474_pin_fixture_init _fini=nx_g474_pin_fixture_fini)
target_compile_options(nexus_g474_pin_state_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/g474_pin_state_model.h")
