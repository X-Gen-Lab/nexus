# Actual U575 providers; CPU/MMIO/NVIC leaves are the only modeled boundaries.
nexus_google_test(nexus_u575_mainstream_test
    SOURCES u575_mainstream_test.cpp u575_watchdog_test.cpp u575_model.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32u575/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32u575/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32u575/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32u575/drivers/watchdog.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32u575/private"
        "${NEXUS_SOURCE_DIR}/tests/google")
target_include_directories(nexus_u575_mainstream_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_u575_mainstream_test PRIVATE STM32U575xx)
target_compile_options(nexus_u575_mainstream_test PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/google/u575_model.h")

set(u5_mainstream_input
    "${NEXUS_SOURCE_DIR}/tests/contracts/u575_assembly/mainstream.toml")
set(u5_mainstream_bundle "${CMAKE_CURRENT_BINARY_DIR}/u575-mainstream")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${u5_mainstream_input}"
    --output "${u5_mainstream_bundle}" RESULT_VARIABLE u5_mainstream_generated
    OUTPUT_VARIABLE u5_mainstream_output ERROR_VARIABLE u5_mainstream_error)
if(NOT u5_mainstream_generated EQUAL 0)
    message(FATAL_ERROR
        "U575 mainstream rejected: ${u5_mainstream_output}${u5_mainstream_error}")
endif()
file(READ "${u5_mainstream_bundle}/input_paths.json" u5_mainstream_inputs)
string(JSON u5_mainstream_count LENGTH "${u5_mainstream_inputs}")
math(EXPR u5_mainstream_last "${u5_mainstream_count} - 1")
foreach(index RANGE 0 ${u5_mainstream_last})
    string(JSON u5_mainstream_name MEMBER "${u5_mainstream_inputs}" ${index})
    string(JSON u5_mainstream_path GET "${u5_mainstream_inputs}"
        "${u5_mainstream_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${u5_mainstream_path}")
endforeach()
nexus_google_test(nexus_u575_mainstream_factory_test
    SOURCES u575_mainstream_factory_test.cpp u575_model.cpp
        "${u5_mainstream_bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32u575/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32u575/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32u575/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32u575/drivers/watchdog.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${u5_mainstream_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32u575/private"
        "${NEXUS_SOURCE_DIR}/tests/google")
target_include_directories(nexus_u575_mainstream_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_u575_mainstream_factory_test PRIVATE STM32U575xx
    _init=nx_u575_fixture_init _fini=nx_u575_fixture_fini)
target_compile_options(nexus_u575_mainstream_factory_test PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/google/u575_mainstream_model.h")
