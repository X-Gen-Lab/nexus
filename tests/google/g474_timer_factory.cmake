# Both fixtures generate actual factory faces and execute production providers.
foreach(g4_timer_kind capture encoder)
    if(g4_timer_kind STREQUAL "capture")
        set(g4_timer_file "timer-capture")
        set(g4_timer_driver "timer_capture")
    else()
        set(g4_timer_file "encoder")
        set(g4_timer_driver "encoder")
    endif()
    set(g4_timer_bundle
        "${CMAKE_CURRENT_BINARY_DIR}/g474-${g4_timer_kind}-factory")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}"
        --assembly
        "${NEXUS_SOURCE_DIR}/tests/contracts/g474_control_assembly/${g4_timer_file}.toml"
        --output "${g4_timer_bundle}" RESULT_VARIABLE g4_timer_generated
        OUTPUT_VARIABLE g4_timer_output ERROR_VARIABLE g4_timer_error)
    if(NOT g4_timer_generated EQUAL 0)
        message(FATAL_ERROR
            "G474 timer fixture rejected: ${g4_timer_output}${g4_timer_error}")
    endif()
    nexus_google_configuration_inputs("${g4_timer_bundle}")
    set(g4_timer_target "nexus_g474_${g4_timer_kind}_factory_test")
    nexus_google_test(${g4_timer_target} SOURCES g474_timer_factory_test.cpp
        g474_timer_factory_ports.c "${g4_timer_bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/platform_cold.c"
        "${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/${g4_timer_driver}.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/${g4_timer_driver}_cold.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${g4_timer_bundle}"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/private"
            "${NEXUS_SOURCE_DIR}/io/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts")
    target_include_directories(${g4_timer_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    nexus_use_cmsis_core(${g4_timer_target})
    target_compile_definitions(${g4_timer_target} PRIVATE STM32G474xx
        "_init=nx_g474_${g4_timer_kind}_init"
        "_fini=nx_g474_${g4_timer_kind}_fini")
    if(g4_timer_kind STREQUAL "capture")
        target_compile_definitions(${g4_timer_target} PRIVATE
            NX_G474_CAPTURE_FACTORY)
    endif()
    target_compile_options(${g4_timer_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/g474_timer_factory_model.h")
endforeach()
