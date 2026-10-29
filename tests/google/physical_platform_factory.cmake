# Execute empty and GPIO-only actual generated lifecycle entries per family.
foreach(physical_family stm32 gd32)
    set(physical_official)
    if(physical_family STREQUAL "stm32")
        set(physical_official STM32F4_LL)
    endif()
    if(physical_family STREQUAL "stm32")
        set(physical_soc stm32f407)
    else()
        set(physical_soc gd32f470)
    endif()
    set(physical_target "nexus_${physical_family}_physical_platform_factory_test")
    foreach(physical_mode empty gpio)
        set(physical_bundle "${CMAKE_CURRENT_BINARY_DIR}/physical-platform-${physical_family}-${physical_mode}")
        execute_process(COMMAND "${Python3_EXECUTABLE}" -B
            "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
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
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
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
            # Generated exports start with a const Nexus return type and the
            # factory name followed by '('. Arguments may continue on a new
            # line. Reject any other export spelling instead of missing it.
            file(READ "${physical_bundle}/nexus_factory.h" physical_factory_header)
            set(physical_factory_pattern
                "const nx_[a-z0-9_]+_t[*] (nx_factory_[a-z0-9_]+)[(]")
            string(REGEX MATCHALL "${physical_factory_pattern}"
                physical_factory_declarations "${physical_factory_header}")
            string(REGEX MATCHALL "nx_factory_[A-Za-z0-9_]+[ \t\r\n]*[(]"
                physical_factory_references "${physical_factory_header}")
            list(LENGTH physical_factory_declarations physical_factory_count)
            list(LENGTH physical_factory_references physical_factory_reference_count)
            if(physical_factory_count EQUAL 0 OR
                    NOT physical_factory_count EQUAL physical_factory_reference_count)
                message(FATAL_ERROR "Unexpected generated factory export declarations")
            endif()
            set(physical_factory_symbols)
            foreach(declaration IN LISTS physical_factory_declarations)
                string(REGEX MATCH "${physical_factory_pattern}"
                    physical_factory_match "${declaration}")
                list(APPEND physical_factory_symbols "${CMAKE_MATCH_1}")
            endforeach()
            list(REMOVE_DUPLICATES physical_factory_symbols)
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
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${physical_gpio_bundle}" "${NEXUS_SOC_SOURCE_${physical_soc}}"
            "${NEXUS_SOC_SOURCE_${physical_soc}}/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
    target_sources(${physical_target} PRIVATE
        $<TARGET_OBJECTS:${physical_target}_empty>
        $<TARGET_OBJECTS:${physical_target}_gpio>)
    target_compile_options(${physical_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/physical_platform_factory_model.h"
        "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
    set(physical_gd "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
    foreach(physical_unit ${physical_target} ${physical_target}_empty
            ${physical_target}_gpio)
        target_include_directories(${physical_unit} SYSTEM PRIVATE
            "${physical_gd}/CMSIS/GD/GD32F4xx/Include"
            "${physical_gd}/GD32F4xx_standard_peripheral/Include")
        if(physical_family STREQUAL "stm32")
            target_include_directories(${physical_unit} SYSTEM PRIVATE
                "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
            target_compile_definitions(${physical_unit} PRIVATE
                STM32F407xx NX_PHYSICAL_FACTORY_STM32)
        else()
            target_compile_definitions(${physical_unit} PRIVATE
                GD32F470 HXTAL_VALUE=25000000)
        endif()
    endforeach()
    if(physical_family STREQUAL "gd32")
        set(physical_vendor "${physical_target}_vendor")
        add_library(${physical_vendor} OBJECT
            "${physical_gd}/GD32F4xx_standard_peripheral/Source/gd32f4xx_gpio.c"
            "${physical_gd}/GD32F4xx_standard_peripheral/Source/gd32f4xx_rcu.c")
        target_include_directories(${physical_vendor} PRIVATE
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model"
            "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/private")
        target_include_directories(${physical_vendor} SYSTEM PRIVATE
            "${physical_gd}/CMSIS/GD/GD32F4xx/Include"
            "${physical_gd}/GD32F4xx_standard_peripheral/Include")
        target_compile_definitions(${physical_vendor} PRIVATE
            GD32F470 HXTAL_VALUE=25000000)
        target_compile_options(${physical_vendor} PRIVATE -Wno-int-to-pointer-cast)
        target_sources(${physical_target} PRIVATE $<TARGET_OBJECTS:${physical_vendor}>)
    endif()
endforeach()
