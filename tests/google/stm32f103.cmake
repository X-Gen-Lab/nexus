# Actual owned F103 code; only external register/IRQ behavior is modeled.
foreach(f1_suite system provider watchdog exti spi platform)
    set(f1_target "nexus_stm32f103_${f1_suite}_test")
    nexus_google_test(${f1_target}
        SOURCES "stm32f103_${f1_suite}_test.cpp"
            "${NEXUS_SOURCE_DIR}/tests/contracts/stm32f103_model/model.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f103/system.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f103/drivers/gpio.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f103/drivers/uart.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f103/drivers/watchdog.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f103/drivers/exti.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f103/drivers/spi.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f103/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts/stm32f103_model")
    target_include_directories(${f1_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    nexus_use_cmsis_core(${f1_target})
    target_compile_definitions(${f1_target} PRIVATE STM32F103xB
        __ARM_ARCH_7M__=1 __ARM_FEATURE_LDREX=7)
    target_compile_options(${f1_target} PRIVATE
        "-include${NEXUS_SOURCE_DIR}/tests/contracts/stm32f103_model/model.h")
endforeach()

# Actual authored F103 assemblies exercise the complete public lifecycle.
foreach(f1_factory io spi)
    if(f1_factory STREQUAL "io")
        set(assembly stm32-f103-baremetal)
    else()
        set(assembly stm32-f103-spi)
    endif()
    set(bundle "${CMAKE_CURRENT_BINARY_DIR}/f103-${f1_factory}-factory")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}" --assembly
        "${NEXUS_SOURCE_DIR}/tools/configure/assemblies/${assembly}.toml"
        --output "${bundle}" RESULT_VARIABLE generated
        OUTPUT_VARIABLE generated_output ERROR_VARIABLE generated_error)
    if(NOT generated EQUAL 0)
        message(FATAL_ERROR "F103 factory rejected: ${generated_output}${generated_error}")
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
    set(target "nexus_stm32f103_${f1_factory}_factory_test")
    nexus_google_test(${target} SOURCES stm32f103_factory_test.cpp
        "${bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/tests/contracts/stm32f103_model/model.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f103/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f103/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f103/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f103/drivers/spi.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${bundle}"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f103/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts/stm32f103_model")
    target_include_directories(${target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f1/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32f1_hal/Inc")
    nexus_use_cmsis_core(${target})
    target_compile_definitions(${target} PRIVATE STM32F103xB
        __ARM_ARCH_7M__=1 __ARM_FEATURE_LDREX=7
        _init=nx_f103_fixture_init _fini=nx_f103_fixture_fini)
    if(f1_factory STREQUAL "spi")
        target_compile_definitions(${target} PRIVATE NX_F103_FACTORY_SPI)
    endif()
    target_compile_options(${target} PRIVATE
        "-include${NEXUS_SOURCE_DIR}/tests/contracts/stm32f103_model/model.h")
endforeach()
