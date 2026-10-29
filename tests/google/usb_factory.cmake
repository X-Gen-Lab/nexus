foreach(usb_family stm32 gd32)
    if(usb_family STREQUAL "stm32")
        set(usb_label "stm32-zg")
        set(usb_soc "stm32f407")
    else()
        set(usb_label "gd32")
        set(usb_soc "gd32f470")
    endif()
    set(usb_canonical_input "${NEXUS_SOURCE_DIR}/tests/contracts/usb_assembly/${usb_label}.toml")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${usb_canonical_input}")
    foreach(usb_capabilities TRUE FALSE)
        if(usb_capabilities)
            set(usb_input "${usb_canonical_input}")
            set(usb_bundle "${CMAKE_CURRENT_BINARY_DIR}/usb-factory-${usb_family}")
            set(usb_target "nexus_${usb_family}_usb_factory_test")
            set(usb_fixture_source usb_factory_test.cpp)
        else()
            file(READ "${usb_canonical_input}" usb_assembly)
            set(usb_board "${NEXUS_SOURCE_DIR}/tests/contracts/usb_assembly/${usb_label}-board")
            string(FIND "${usb_assembly}" "board_package=\"${usb_label}-board\"" usb_board_key)
            string(FIND "${usb_assembly}" "capabilities=true" usb_capability_key)
            if(usb_board_key EQUAL -1 OR usb_capability_key EQUAL -1)
                message(FATAL_ERROR "USB factory variant requires its canonical board and capabilities=true")
            endif()
            string(REPLACE "board_package=\"${usb_label}-board\""
                "board_package=\"${usb_board}\"" usb_assembly "${usb_assembly}")
            string(REPLACE "capabilities=true" "capabilities=false"
                usb_assembly "${usb_assembly}")
            file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/usb-factory-inputs")
            set(usb_input "${CMAKE_CURRENT_BINARY_DIR}/usb-factory-inputs/${usb_family}-no-capabilities.toml")
            file(CONFIGURE OUTPUT "${usb_input}" CONTENT "${usb_assembly}" @ONLY)
            set(usb_bundle "${CMAKE_CURRENT_BINARY_DIR}/usb-factory-${usb_family}-no-capabilities")
            set(usb_target "nexus_${usb_family}_usb_activation_factory_no_capabilities_test")
            set(usb_fixture_source usb_factory_activation_test.cpp)
        endif()
        execute_process(COMMAND "${Python3_EXECUTABLE}" -B
            "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
            --source-root "${NEXUS_SOURCE_DIR}" --assembly "${usb_input}"
            --output "${usb_bundle}" RESULT_VARIABLE usb_generated
            OUTPUT_VARIABLE usb_output ERROR_VARIABLE usb_error)
        if(NOT usb_generated EQUAL 0)
            message(FATAL_ERROR "USB factory fixture rejected: ${usb_output}${usb_error}")
        endif()
        nexus_google_configuration_inputs("${usb_bundle}")
        nexus_google_test(${usb_target} SOURCES ${usb_fixture_source}
            "${usb_bundle}/bindings.c"
            "${NEXUS_SOC_SOURCE_${usb_soc}}/drivers/usb_device.c"
            "${NEXUS_SOC_SOURCE_${usb_soc}}/usb_clock.c"
            LIBRARIES Nexus::NativeModel Nexus::Arch
            INCLUDES "${usb_bundle}" "${NEXUS_SOURCE_DIR}/io/private"
                "${NEXUS_SOURCE_DIR}/soc/native/drivers"
                "${NEXUS_SOC_SOURCE_${usb_soc}}/private")
        target_compile_options(${usb_target} PRIVATE
            "-include${CMAKE_CURRENT_SOURCE_DIR}/usb_device_soc_model.h"
            "-include${CMAKE_CURRENT_SOURCE_DIR}/usb_factory_model.h")
        target_compile_definitions(${usb_target} PRIVATE
            _init=nx_usb_fixture_init _fini=nx_usb_fixture_fini)
        if(usb_family STREQUAL "stm32")
        target_sources(${usb_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/platform_cold.c")
            target_include_directories(${usb_target} SYSTEM PRIVATE
                "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
                "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
            nexus_use_cmsis_core(${usb_target})
            target_compile_definitions(${usb_target} PRIVATE STM32F407xx NX_USB_FACTORY_STM32)
        else()
            target_sources(${usb_target} PRIVATE
                "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/drivers/platform_cold.c")
            set(usb_gd "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
            target_include_directories(${usb_target} PRIVATE
                "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
            target_include_directories(${usb_target} SYSTEM PRIVATE
                "${usb_gd}/CMSIS/GD/GD32F4xx/Include"
                "${usb_gd}/GD32F4xx_standard_peripheral/Include"
                "${usb_gd}/GD32F4xx_usb_library/driver/Include")
            target_compile_definitions(${usb_target} PRIVATE GD32F470 HXTAL_VALUE=25000000 NX_USB_GD_MODEL)
            target_compile_options(${usb_target} PRIVATE
                "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
        endif()
    endforeach()
endforeach()
