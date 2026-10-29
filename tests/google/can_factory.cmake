foreach(can_family stm32 gd32)
    set(can_input "${NEXUS_SOURCE_DIR}/tests/contracts/can_assembly/${can_family}-both.toml")
    set(can_bundle "${CMAKE_CURRENT_BINARY_DIR}/can-factory-${can_family}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
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
            "${NEXUS_SOURCE_DIR}/soc/native/drivers")
    target_compile_options(${can_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/can_soc_model.h")
    target_compile_definitions(${can_target} PRIVATE
        _init=nx_can_fixture_init _fini=nx_can_fixture_fini)
    if(can_family STREQUAL "stm32")
        target_sources(${can_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/platform_cold.c")
        target_sources(${can_target} PRIVATE "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/can.c")
        target_include_directories(${can_target} PRIVATE "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private")
        target_include_directories(${can_target} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
            "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
        nexus_use_cmsis_core(${can_target})
        target_compile_definitions(${can_target} PRIVATE STM32F407xx NX_CAN_TEST_STM32 NX_CAN_FACTORY_STM32)
    else()
        target_sources(${can_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/drivers/platform_cold.c")
        set(can_gd "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
        target_sources(${can_target} PRIVATE "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/drivers/can.c")
        target_include_directories(${can_target} PRIVATE "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/private")
        target_include_directories(${can_target} SYSTEM PRIVATE
            "${can_gd}/CMSIS/GD/GD32F4xx/Include"
            "${can_gd}/GD32F4xx_standard_peripheral/Include")
        target_compile_definitions(${can_target} PRIVATE GD32F470 HXTAL_VALUE=25000000)
        target_include_directories(${can_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
        target_compile_options(${can_target} PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
    endif()
endforeach()
