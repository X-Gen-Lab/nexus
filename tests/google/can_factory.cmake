foreach(can_family stm32)
    set(can_input "${NEXUS_SOURCE_DIR}/tests/contracts/can_assembly/${can_family}-both.toml")
    set(can_bundle "${CMAKE_CURRENT_BINARY_DIR}/can-factory-${can_family}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py" --test-support
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${can_input}"
        --output "${can_bundle}" RESULT_VARIABLE can_generated
        OUTPUT_VARIABLE can_output ERROR_VARIABLE can_error)
    if(NOT can_generated EQUAL 0)
        message(FATAL_ERROR "CAN factory fixture rejected: ${can_output}${can_error}")
    endif()
    nexus_google_configuration_inputs("${can_bundle}")
    set(can_target "nexus_${can_family}_can_factory_test")
    nexus_google_test(${can_target} SOURCES can_factory_test.cpp
        "${can_bundle}/bindings.c" LIBRARIES Nexus::NativeModel Nexus::Arch
        INCLUDES "${can_bundle}" "${NEXUS_SOURCE_DIR}/io/private"
            "${NEXUS_SOURCE_DIR}/tests/support/native/drivers")
    target_compile_options(${can_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/can_soc_model.h")
    target_compile_definitions(${can_target} PRIVATE
        _init=nx_can_fixture_init _fini=nx_can_fixture_fini)
    target_sources(${can_target} PRIVATE
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_lifecycle.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_coordinator.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/system_lifecycle.c")
    target_sources(${can_target} PRIVATE "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/can.c")
    target_include_directories(${can_target} PRIVATE "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
    target_include_directories(${can_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    nexus_use_cmsis_core(${can_target})
    target_compile_definitions(${can_target} PRIVATE STM32F407xx NX_CAN_TEST_STM32 NX_CAN_FACTORY_STM32)
endforeach()
