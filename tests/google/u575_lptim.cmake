# Real production LPTIM2 code, external APB/clock/vector leaves only.
nexus_google_test(nexus_u575_lptim_test
    SOURCES u575_lptim_test.cpp u575_lptim_model.cpp u575_model.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/lptim.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/private"
        "${NEXUS_SOURCE_DIR}/tests/google")
target_include_directories(nexus_u575_lptim_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_u575_lptim_test)
target_compile_definitions(nexus_u575_lptim_test PRIVATE STM32U575xx)
target_compile_options(nexus_u575_lptim_test PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/google/u575_lptim_model.h")

set(u575_lptim_input "${NEXUS_SOURCE_DIR}/tools/configure/assemblies/u575-lptim-run.toml")
set(u575_lptim_bundle "${CMAKE_CURRENT_BINARY_DIR}/u575-lptim-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${u575_lptim_input}"
    --output "${u575_lptim_bundle}" RESULT_VARIABLE u575_lptim_generated
    OUTPUT_VARIABLE u575_lptim_output ERROR_VARIABLE u575_lptim_error)
if(NOT u575_lptim_generated EQUAL 0)
    message(FATAL_ERROR "U575 LPTIM assembly rejected: ${u575_lptim_output}${u575_lptim_error}")
endif()
nexus_google_configuration_inputs("${u575_lptim_bundle}")
nexus_google_test(nexus_u575_lptim_factory_test
    SOURCES u575_lptim_factory_test.cpp u575_lptim_model.cpp u575_model.cpp
        "${u575_lptim_bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/platform_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/lptim.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${u575_lptim_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/private"
        "${NEXUS_SOURCE_DIR}/tests/google")
target_include_directories(nexus_u575_lptim_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_u575_lptim_factory_test)
target_compile_definitions(nexus_u575_lptim_factory_test PRIVATE STM32U575xx
    _init=nx_u575_lptim_fixture_init _fini=nx_u575_lptim_fixture_fini)
target_compile_options(nexus_u575_lptim_factory_test PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/google/u575_lptim_factory_model.h")
