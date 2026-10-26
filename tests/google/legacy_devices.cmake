# Shared reviewed engines execute real owned C against external MMIO models.
nexus_google_test(nexus_legacy_devices_test
    SOURCES legacy_devices_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/common/legacy_flash.c"
        "${NEXUS_SOURCE_DIR}/soc/common/legacy_adc.c"
        "${NEXUS_SOURCE_DIR}/soc/common/legacy_pwm.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/common")
target_compile_options(nexus_legacy_devices_test PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/contracts/legacy_devices_model.h")

function(nexus_legacy_device_factory family)
    if(family STREQUAL "stm32f103")
        set(short f103)
        set(definitions STM32F103xB __ARM_ARCH_7M__=1 __ARM_FEATURE_LDREX=7
            NX_LEGACY_FACTORY_F103)
        set(vendor "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
            "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc")
    else()
        set(short gd303)
        set(definitions GD32F30X_HD)
        set(vendor
            "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f30x/Firmware/CMSIS/GD/GD32F30x/Include"
            "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f30x/Firmware/GD32F30x_standard_peripheral/Include")
    endif()
    set(bundle "${CMAKE_CURRENT_BINARY_DIR}/${short}-devices-factory")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}"
        --assembly
        "${NEXUS_SOURCE_DIR}/tests/contracts/mainstream_assembly/${short}-devices.toml"
        --output "${bundle}"
        RESULT_VARIABLE generated OUTPUT_VARIABLE generated_output
        ERROR_VARIABLE generated_error)
    if(NOT generated EQUAL 0)
        message(FATAL_ERROR "Legacy device factory rejected: ${generated_output}${generated_error}")
    endif()
    foreach(catalog input_paths configuration_code_paths)
        file(READ "${bundle}/${catalog}.json" inputs)
        string(JSON count LENGTH "${inputs}")
        math(EXPR last "${count} - 1")
        foreach(index RANGE 0 ${last})
            string(JSON label MEMBER "${inputs}" ${index})
            string(JSON path GET "${inputs}" "${label}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
        endforeach()
    endforeach()
    set(target "nexus_${short}_devices_factory_test")
    set(exti)
    if(family STREQUAL "gd32f303")
        set(exti "${NEXUS_SOC_SOURCE_${family}}/drivers/exti.c")
    endif()
    nexus_google_test(${target}
        SOURCES legacy_devices_factory_test.cpp "${bundle}/bindings.c"
            "${NEXUS_SOURCE_DIR}/tests/contracts/${family}_model/model.c"
            "${NEXUS_SOC_SOURCE_${family}}/system.c"
            "${NEXUS_SOC_SOURCE_${family}}/drivers/gpio.c"
            "${NEXUS_SOC_SOURCE_${family}}/drivers/devices.c" ${exti}
            "${NEXUS_SOURCE_DIR}/soc/common/legacy_adc.c"
            "${NEXUS_SOURCE_DIR}/soc/common/legacy_pwm.c"
            "${NEXUS_SOURCE_DIR}/soc/common/legacy_flash.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${bundle}" "${NEXUS_SOC_SOURCE_${family}}/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts/${family}_model")
    target_include_directories(${target} SYSTEM PRIVATE ${vendor}
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    nexus_use_cmsis_core(${target})
    target_compile_definitions(${target} PRIVATE ${definitions}
        _init=nx_legacy_fixture_init _fini=nx_legacy_fixture_fini)
    target_compile_options(${target} PRIVATE
        "-include${NEXUS_SOURCE_DIR}/tests/contracts/${family}_model/model.h")
endfunction()
nexus_legacy_device_factory(stm32f103)
nexus_legacy_device_factory(gd32f303)


# Existing exact F407/GD470 production Flash engines honor the same pulse ABI.
function(nexus_physical_flash_program_test family)
    if(family STREQUAL "stm32")
        set(soc stm32f407)
        set(definitions NEXUS_STM32_MODEL STM32F407xx NX_FLASH_PROGRAM_STM32)
        set(includes "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
            "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    else()
        set(soc gd32f470)
        set(definitions GD32F470 HXTAL_VALUE=25000000)
        set(includes
            "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware/CMSIS/GD/GD32F4xx/Include"
            "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware/GD32F4xx_standard_peripheral/Include")
    endif()
    set(bundle "${CMAKE_CURRENT_BINARY_DIR}/${family}-flash-factory")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}" --assembly
        "${NEXUS_SOURCE_DIR}/tests/contracts/flash_factory/${family}.toml"
        --output "${bundle}" RESULT_VARIABLE generated
        OUTPUT_VARIABLE generated_output ERROR_VARIABLE generated_error)
    if(NOT generated EQUAL 0)
        message(FATAL_ERROR "Physical Flash factory rejected: ${generated_output}${generated_error}")
    endif()
    foreach(catalog input_paths configuration_code_paths)
        file(READ "${bundle}/${catalog}.json" inputs)
        string(JSON count LENGTH "${inputs}")
        math(EXPR last "${count} - 1")
        foreach(index RANGE 0 ${last})
            string(JSON label MEMBER "${inputs}" ${index})
            string(JSON path GET "${inputs}" "${label}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
        endforeach()
    endforeach()
    set(target "nexus_${family}_flash_program_test")
    nexus_google_test(${target} SOURCES flash_program_test.cpp
        flash_capability_test.cpp "${bundle}/bindings.c"
        "${NEXUS_SOC_SOURCE_${soc}}/drivers/flash.c"
        LIBRARIES Nexus::NativeModel Nexus::Arch
        INCLUDES "${bundle}" "${NEXUS_SOC_SOURCE_${soc}}"
            "${NEXUS_SOC_SOURCE_${soc}}/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
    target_include_directories(${target} SYSTEM PRIVATE ${includes})
    nexus_use_cmsis_core(${target})
    target_compile_definitions(${target} PRIVATE ${definitions}
        _init=nx_flash_fixture_init _fini=nx_flash_fixture_fini)
    target_compile_options(${target} PRIVATE
        "-include${NEXUS_SOURCE_DIR}/tests/contracts/flash_program_model.h"
        "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
endfunction()
nexus_physical_flash_program_test(stm32)
nexus_physical_flash_program_test(gd32)
