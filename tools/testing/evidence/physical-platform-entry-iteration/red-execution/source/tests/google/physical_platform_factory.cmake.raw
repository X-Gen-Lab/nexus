# Execute empty and GPIO-only actual generated lifecycle entries per family.
file(GLOB_RECURSE physical_factory_codegen CONFIGURE_DEPENDS
    "${NEXUS_SOURCE_DIR}/tools/configure/*.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${physical_factory_codegen})
set(physical_kinds gpio uart spi i2c flash watchdog exti pwm adc can usb_device
    eth timer_capture advanced_pwm spi_device i2c_device usb_endpoint mdio)
foreach(physical_family stm32 gd32)
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
        file(READ "${physical_bundle}/input_paths.json" physical_inputs)
        string(JSON physical_count LENGTH "${physical_inputs}")
        math(EXPR physical_last "${physical_count} - 1")
        foreach(index RANGE 0 ${physical_last})
            string(JSON label MEMBER "${physical_inputs}" ${index})
            string(JSON input_path GET "${physical_inputs}" "${label}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input_path}")
        endforeach()
        set(physical_object "${physical_target}_${physical_mode}")
        add_library(${physical_object} OBJECT "${physical_bundle}/bindings.c")
        target_include_directories(${physical_object} PRIVATE
            "${physical_bundle}" "${NEXUS_SOURCE_DIR}/soc/${physical_soc}"
            "${NEXUS_SOURCE_DIR}/soc/${physical_soc}/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
        target_link_libraries(${physical_object} PRIVATE Nexus::IO Nexus::Arch)
        target_compile_definitions(${physical_object} PRIVATE
            _init=nx_physical_${physical_mode}_init
            _fini=nx_physical_${physical_mode}_fini)
        if(physical_mode STREQUAL "empty")
            target_compile_definitions(${physical_object} PRIVATE
                nx_platform_start=nx_empty_platform_start
                nx_platform_stop=nx_empty_platform_stop)
            foreach(kind IN LISTS physical_kinds)
                target_compile_definitions(${physical_object} PRIVATE
                    nx_factory_${kind}=nx_empty_factory_${kind}
                    nx_factory_${kind}_capability=nx_empty_factory_${kind}_capability)
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
    nexus_google_test(${physical_target} SOURCES physical_platform_factory_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/${physical_soc}/drivers/gpio.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${physical_gpio_bundle}" "${NEXUS_SOURCE_DIR}/soc/${physical_soc}"
            "${NEXUS_SOURCE_DIR}/soc/${physical_soc}/private"
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
            "${NEXUS_SOURCE_DIR}/soc/gd32f470/private")
        target_include_directories(${physical_vendor} SYSTEM PRIVATE
            "${physical_gd}/CMSIS/GD/GD32F4xx/Include"
            "${physical_gd}/GD32F4xx_standard_peripheral/Include")
        target_compile_definitions(${physical_vendor} PRIVATE
            GD32F470 HXTAL_VALUE=25000000)
        target_compile_options(${physical_vendor} PRIVATE -Wno-int-to-pointer-cast)
        target_sources(${physical_target} PRIVATE $<TARGET_OBJECTS:${physical_vendor}>)
    endif()
endforeach()
