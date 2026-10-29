# One real U575 assembly; only external CPU and MMIO leaves are modeled.
set(u5_factory_input "${NEXUS_SOURCE_DIR}/tests/contracts/u575_assembly/io.toml")
set(u5_factory_bundle "${CMAKE_CURRENT_BINARY_DIR}/u575-factory")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/configure/__init__.py"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/configure/bindings.py"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/configure/emission.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${u5_factory_input}"
    --output "${u5_factory_bundle}" RESULT_VARIABLE u5_generated
    OUTPUT_VARIABLE u5_output ERROR_VARIABLE u5_error)
if(NOT u5_generated EQUAL 0)
    message(FATAL_ERROR "U575 factory fixture rejected: ${u5_output}${u5_error}")
endif()
nexus_google_configuration_inputs("${u5_factory_bundle}")
nexus_google_test(nexus_u575_factory_test SOURCES u575_factory_test.cpp
    "${u5_factory_bundle}/bindings.c"
    "${NEXUS_SOURCE_DIR}/tests/google/u575_model.cpp"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/platform_cold.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/gpio.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/uart.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${u5_factory_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/private"
        "${NEXUS_SOURCE_DIR}/tests/google")
target_include_directories(nexus_u575_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_u575_factory_test)
target_compile_definitions(nexus_u575_factory_test PRIVATE STM32U575xx
    _init=nx_u575_fixture_init _fini=nx_u575_fixture_fini)
target_compile_options(nexus_u575_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/u575_factory_model.h")
