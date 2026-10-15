# Execute the actual atomic generated bundle and exact SDK timer providers.
file(GLOB_RECURSE timer_codegen_sources CONFIGURE_DEPENDS
    "${NEXUS_SOURCE_DIR}/tools/configure/*.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${timer_codegen_sources})
foreach(timer_family stm32 gd32)
    if(timer_family STREQUAL "stm32")
        set(timer_label "stm32-zg")
        set(timer_soc "stm32f407")
    else()
        set(timer_label "gd32")
        set(timer_soc "gd32f470")
    endif()
    set(timer_input "${NEXUS_SOURCE_DIR}/tests/contracts/timer_assembly/${timer_label}.toml")
    set(timer_bundle "${CMAKE_CURRENT_BINARY_DIR}/timer-factory-${timer_family}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${timer_input}"
        --output "${timer_bundle}" RESULT_VARIABLE timer_generated
        OUTPUT_VARIABLE timer_output ERROR_VARIABLE timer_error)
    if(NOT timer_generated EQUAL 0)
        message(FATAL_ERROR "Timer factory fixture rejected: ${timer_output}${timer_error}")
    endif()
    file(READ "${timer_bundle}/input_paths.json" timer_inputs)
    string(JSON timer_input_count LENGTH "${timer_inputs}")
    math(EXPR timer_last_input "${timer_input_count} - 1")
    foreach(index RANGE 0 ${timer_last_input})
        string(JSON timer_input_name MEMBER "${timer_inputs}" ${index})
        string(JSON timer_input_path GET "${timer_inputs}" "${timer_input_name}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${timer_input_path}")
    endforeach()
    set(timer_target "nexus_${timer_family}_timer_factory_test")
    nexus_google_test(${timer_target} SOURCES timer_factory_test.cpp
        "${timer_bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/soc/${timer_soc}/drivers/timer_capture.c"
        "${NEXUS_SOURCE_DIR}/soc/${timer_soc}/drivers/advanced_pwm.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${timer_bundle}" "${NEXUS_SOURCE_DIR}/io/private"
            "${NEXUS_SOURCE_DIR}/soc/${timer_soc}/private")
    target_compile_options(${timer_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/timer_factory_model.h")
    target_compile_definitions(${timer_target} PRIVATE
        _init=nx_timer_fixture_init _fini=nx_timer_fixture_fini)
    if(timer_family STREQUAL "stm32")
        target_include_directories(${timer_target} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
            "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
        target_compile_definitions(${timer_target} PRIVATE
            STM32F407xx NX_TIMER_FACTORY_STM32)
    else()
        set(timer_gd "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
        target_include_directories(${timer_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
        target_include_directories(${timer_target} SYSTEM PRIVATE
            "${timer_gd}/CMSIS/GD/GD32F4xx/Include"
            "${timer_gd}/GD32F4xx_standard_peripheral/Include")
        target_compile_definitions(${timer_target} PRIVATE
            GD32F470 HXTAL_VALUE=25000000 NX_TIMER_FACTORY_GD32)
        target_compile_options(${timer_target} PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
    endif()
endforeach()
