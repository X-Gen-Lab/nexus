# Include after actual authored/IR admission and provider RED/GREEN.
# Every target uses its normal TOML bundle and real production definitions.
foreach(profile regular-software regular-timer injected-software injected-timer)
    string(REPLACE "-" "_" target_suffix "${profile}")
    set(group_input
        "${NEXUS_SOURCE_DIR}/cmake/package/verification/io_evolution/g474-adc-group-${profile}.toml")
    set(group_bundle
        "${CMAKE_CURRENT_BINARY_DIR}/g474-adc-group-${profile}-factory")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${group_input}"
        --output "${group_bundle}" RESULT_VARIABLE group_generated
        OUTPUT_VARIABLE group_output ERROR_VARIABLE group_error)
    if(NOT group_generated EQUAL 0)
        message(FATAL_ERROR
            "G474 group fixture rejected: ${group_output}${group_error}")
    endif()
    file(READ "${group_bundle}/input_paths.json" group_inputs)
    string(JSON group_input_count LENGTH "${group_inputs}")
    math(EXPR group_last_input "${group_input_count} - 1")
    foreach(index RANGE 0 ${group_last_input})
        string(JSON group_name MEMBER "${group_inputs}" ${index})
        string(JSON group_path GET "${group_inputs}" "${group_name}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${group_path}")
    endforeach()
    set(group_target "nexus_g474_adc_group_${target_suffix}_factory_test")
    nexus_google_test("${group_target}"
        SOURCES g474_adc_group_factory_test.cpp
            g474_adc_group_admission_factory_test.cpp
            g474_adc_group_factory_ports.cpp g474_adc_group_model.cpp
            "${group_bundle}/bindings.c"
            "${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32g474/system.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32g474/drivers/gpio.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32g474/drivers/adc_group.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${group_bundle}"
            "${NEXUS_SOURCE_DIR}/soc/stm32g474/private"
            "${NEXUS_SOURCE_DIR}/soc/common"
            "${NEXUS_SOURCE_DIR}/io/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts")
    include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
    nexus_stm32_official("${group_target}" g4)
    target_include_directories("${group_target}" SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
        "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
    target_compile_definitions("${group_target}" PRIVATE STM32G474xx
        NX_G474_ADC_GROUP_EXTERNAL_CORE
        _init=nx_g474_adc_group_fixture_init _fini=nx_g474_adc_group_fixture_fini)
    if(profile MATCHES "^injected-")
        target_compile_definitions("${group_target}" PRIVATE NX_AG_FACTORY_INJECTED)
    endif()
    if(profile MATCHES "-software$")
        target_compile_definitions("${group_target}" PRIVATE NX_AG_FACTORY_SOFTWARE)
    endif()
    target_compile_options("${group_target}" PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/g474_adc_group_factory_model.h")
endforeach()
