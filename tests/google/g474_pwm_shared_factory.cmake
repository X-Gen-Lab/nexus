# One canonical normal input, real generated Platform/System/provider lifecycle.
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
set(g4_shared_input
    "${NEXUS_SOURCE_DIR}/cmake/package/verification/io_evolution/g474-pwm-shared.toml")
set(g4_shared_bundle "${CMAKE_CURRENT_BINARY_DIR}/g474-pwm-shared-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${g4_shared_input}"
    --output "${g4_shared_bundle}" RESULT_VARIABLE g4_shared_generated
    OUTPUT_VARIABLE g4_shared_output ERROR_VARIABLE g4_shared_error)
if(NOT g4_shared_generated EQUAL 0)
    message(FATAL_ERROR
        "Shared TIM3 factory rejected: ${g4_shared_output}${g4_shared_error}")
endif()
file(READ "${g4_shared_bundle}/input_paths.json" g4_shared_inputs)
string(JSON g4_shared_count LENGTH "${g4_shared_inputs}")
math(EXPR g4_shared_last "${g4_shared_count} - 1")
foreach(index RANGE 0 ${g4_shared_last})
    string(JSON g4_shared_name MEMBER "${g4_shared_inputs}" ${index})
    string(JSON g4_shared_path GET "${g4_shared_inputs}" "${g4_shared_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${g4_shared_path}")
endforeach()
nexus_google_test(nexus_g474_pwm_shared_factory_test
    SOURCES g474_pwm_shared_factory_test.cpp
        g474_pwm_shared_factory_ports.cpp g474_pwm_shared_model.cpp
        "${g4_shared_bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/pwm_shared.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${g4_shared_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts")
nexus_stm32_official(nexus_g474_pwm_shared_factory_test g4)
target_include_directories(nexus_g474_pwm_shared_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_g474_pwm_shared_factory_test PRIVATE
    STM32G474xx _init=nx_g474_shared_factory_init
    _fini=nx_g474_shared_factory_fini)
target_compile_options(nexus_g474_pwm_shared_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/g474_pwm_shared_factory_model.h")
