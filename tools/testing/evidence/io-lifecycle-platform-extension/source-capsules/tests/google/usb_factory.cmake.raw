foreach(usb_family stm32 gd32)
    if(usb_family STREQUAL "stm32")
        set(usb_label "stm32-zg")
        set(usb_soc "stm32f407")
    else()
        set(usb_label "gd32")
        set(usb_soc "gd32f470")
    endif()
    set(usb_input "${NEXUS_SOURCE_DIR}/tests/contracts/usb_assembly/${usb_label}.toml")
    set(usb_bundle "${CMAKE_CURRENT_BINARY_DIR}/usb-factory-${usb_family}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${usb_input}"
        --output "${usb_bundle}" RESULT_VARIABLE usb_generated
        OUTPUT_VARIABLE usb_output ERROR_VARIABLE usb_error)
    if(NOT usb_generated EQUAL 0)
        message(FATAL_ERROR "USB factory fixture rejected: ${usb_output}${usb_error}")
    endif()
    file(READ "${usb_bundle}/input_paths.json" usb_inputs)
    string(JSON usb_input_count LENGTH "${usb_inputs}")
    math(EXPR usb_last_input "${usb_input_count} - 1")
    foreach(index RANGE 0 ${usb_last_input})
        string(JSON usb_input_name MEMBER "${usb_inputs}" ${index})
        string(JSON usb_input_path GET "${usb_inputs}" "${usb_input_name}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${usb_input_path}")
    endforeach()
    set(usb_target "nexus_${usb_family}_usb_factory_test")
    nexus_google_test(${usb_target} SOURCES usb_factory_test.cpp
        "${usb_bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/soc/${usb_soc}/drivers/usb_device.c"
        "${NEXUS_SOURCE_DIR}/soc/${usb_soc}/usb_clock.c"
        LIBRARIES Nexus::NativeModel Nexus::Arch
        INCLUDES "${usb_bundle}" "${NEXUS_SOURCE_DIR}/io/private"
            "${NEXUS_SOURCE_DIR}/io/native"
            "${NEXUS_SOURCE_DIR}/soc/${usb_soc}/private")
    target_compile_options(${usb_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/usb_device_soc_model.h"
        "-include${CMAKE_CURRENT_SOURCE_DIR}/usb_factory_model.h")
    target_compile_definitions(${usb_target} PRIVATE
        _init=nx_usb_fixture_init _fini=nx_usb_fixture_fini)
    if(usb_family STREQUAL "stm32")
        target_include_directories(${usb_target} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
            "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
        target_compile_definitions(${usb_target} PRIVATE STM32F407xx NX_USB_FACTORY_STM32)
    else()
        set(usb_gd "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
        target_include_directories(${usb_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
        target_include_directories(${usb_target} SYSTEM PRIVATE
            "${usb_gd}/CMSIS" "${usb_gd}/CMSIS/GD/GD32F4xx/Include"
            "${usb_gd}/GD32F4xx_standard_peripheral/Include"
            "${usb_gd}/GD32F4xx_usb_library/driver/Include")
        target_compile_definitions(${usb_target} PRIVATE GD32F470 HXTAL_VALUE=25000000 NX_USB_GD_MODEL)
        target_compile_options(${usb_target} PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
    endif()
endforeach()
