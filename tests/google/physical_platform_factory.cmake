# Execute empty and GPIO-only actual generated lifecycle entries per family.
foreach(physical_family stm32)
    set(physical_official)
    set(physical_official STM32F4_LL)
    set(physical_soc stm32f4)
    set(physical_target "nexus_${physical_family}_physical_platform_factory_test")
    foreach(physical_mode empty gpio)
        set(physical_bundle "${CMAKE_CURRENT_BINARY_DIR}/physical-platform-${physical_family}-${physical_mode}")
        execute_process(COMMAND "${Python3_EXECUTABLE}" -B
            "${NEXUS_SOURCE_DIR}/tools/configure/configure.py" --test-support
            --source-root "${NEXUS_SOURCE_DIR}" --assembly
            "${NEXUS_SOURCE_DIR}/tests/contracts/physical_platform_assembly/${physical_family}-${physical_mode}.toml"
            --output "${physical_bundle}" RESULT_VARIABLE physical_generated
            OUTPUT_VARIABLE physical_output ERROR_VARIABLE physical_error)
        if(NOT physical_generated EQUAL 0)
            message(FATAL_ERROR "Physical platform fixture rejected: ${physical_output}${physical_error}")
        endif()
        nexus_google_configuration_inputs("${physical_bundle}")
        set(physical_object "${physical_target}_${physical_mode}")
        add_library(${physical_object} OBJECT "${physical_bundle}/bindings.c")
        target_include_directories(${physical_object} PRIVATE
            "${physical_bundle}" "${NEXUS_SOC_SOURCE_${physical_soc}}"
            "${NEXUS_SOC_SOURCE_${physical_soc}}/private"
            "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu")
        target_link_libraries(${physical_object} PRIVATE Nexus::IO Nexus::Arch)
        target_compile_definitions(${physical_object} PRIVATE
            _init=nx_physical_${physical_mode}_init
            _fini=nx_physical_${physical_mode}_fini)
        if(physical_mode STREQUAL "empty")
            target_compile_definitions(${physical_object} PRIVATE
                nx_platform_start=nx_empty_platform_start
                nx_platform_stop=nx_empty_platform_stop
                nx_platform_start_diagnosed=nx_empty_platform_start_diagnosed
                nx_platform_stop_diagnosed=nx_empty_platform_stop_diagnosed)
            # Generated exports return a const Nexus pointer or immutable label
            # text, followed by the factory name and '('. Arguments may continue
            # on a new line. Reject any other export spelling instead of missing it.
            include("${CMAKE_CURRENT_LIST_DIR}/factory_symbols.cmake")
            nexus_google_factory_symbols(physical_factory_symbols
                "${physical_bundle}/nexus_factory.h")
            foreach(symbol IN LISTS physical_factory_symbols)
                string(REGEX REPLACE "^nx_" "nx_empty_" renamed "${symbol}")
                target_compile_definitions(${physical_object} PRIVATE "${symbol}=${renamed}")
            endforeach()
        else()
            set(physical_gpio_bundle "${physical_bundle}")
        endif()
        target_compile_options(${physical_object} PRIVATE
            "-include${CMAKE_CURRENT_SOURCE_DIR}/physical_platform_factory_model.h"
            "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
        if(TARGET nexus_build_options)
            target_link_libraries(${physical_object} PRIVATE
                nexus_build_options nexus_owned_warnings)
        endif()
    endforeach()
    nexus_google_test(${physical_target} ${physical_official}
        SOURCES physical_platform_factory_test.cpp
        "${NEXUS_SOC_SOURCE_${physical_soc}}/drivers/gpio.c"
        "${NEXUS_SOC_SOURCE_${physical_soc}}/drivers/platform_cold.c"
        "${NEXUS_SOC_SOURCE_${physical_soc}}/drivers/platform_lifecycle.c"
        "${NEXUS_SOC_SOURCE_${physical_soc}}/drivers/platform_coordinator.c"
        "${NEXUS_SOC_SOURCE_${physical_soc}}/drivers/system_lifecycle.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${physical_gpio_bundle}" "${NEXUS_SOC_SOURCE_${physical_soc}}"
            "${NEXUS_SOC_SOURCE_${physical_soc}}/private"
            "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu")
    target_sources(${physical_target} PRIVATE
        $<TARGET_OBJECTS:${physical_target}_empty>
        $<TARGET_OBJECTS:${physical_target}_gpio>)
    target_compile_options(${physical_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/physical_platform_factory_model.h"
        "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
    foreach(physical_unit ${physical_target} ${physical_target}_empty
            ${physical_target}_gpio)
        target_include_directories(${physical_unit} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
        target_compile_definitions(${physical_unit} PRIVATE
            STM32F407xx NX_PHYSICAL_FACTORY_STM32)
    endforeach()
endforeach()
