# Execute the real generated five-controller graph with exact SDK providers.
foreach(multi_family stm32)
    set(multi_official)
    set(multi_official STM32F4_LL)
    set(multi_label stm32-zg)
    set(multi_soc stm32f4)
    set(multi_uart_tx uart_hal)
    set(multi_model multi_controller_hal_model.h)
    set(multi_input "${NEXUS_SOURCE_DIR}/tests/contracts/multi_controller_assembly/${multi_label}.toml")
    set(multi_bundle "${CMAKE_CURRENT_BINARY_DIR}/multi-controller-factory-${multi_family}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py" --test-support
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${multi_input}"
        --output "${multi_bundle}" RESULT_VARIABLE multi_generated
        OUTPUT_VARIABLE multi_output ERROR_VARIABLE multi_error)
    if(NOT multi_generated EQUAL 0)
        message(FATAL_ERROR "Five-controller factory rejected: ${multi_output}${multi_error}")
    endif()
    nexus_google_configuration_inputs("${multi_bundle}")
    set(multi_target "nexus_${multi_family}_multi_controller_factory_test")
    nexus_google_test(${multi_target} ${multi_official}
        SOURCES multi_controller_factory_test.cpp
        "${multi_bundle}/bindings.c"
        "${NEXUS_SOC_SOURCE_${multi_soc}}/drivers/uart.c"
        "${NEXUS_SOC_SOURCE_${multi_soc}}/drivers/${multi_uart_tx}.c"
        "${NEXUS_SOC_SOURCE_${multi_soc}}/drivers/spi.c"
        "${NEXUS_SOC_SOURCE_${multi_soc}}/drivers/spi_dma.c"
        "${NEXUS_SOC_SOURCE_${multi_soc}}/drivers/i2c_async.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ip/i2c_v1_async/i2c_v1_async.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${multi_bundle}" "${NEXUS_SOURCE_DIR}/io/private"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ip/i2c_v1_async"
            "${NEXUS_SOC_SOURCE_${multi_soc}}"
            "${NEXUS_SOC_SOURCE_${multi_soc}}/private"
            "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu")
    target_sources(${multi_target} PRIVATE
        "${NEXUS_SOC_SOURCE_${multi_soc}}/drivers/i2c_cold.c")
    target_compile_options(${multi_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/${multi_model}"
        "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
    target_compile_definitions(${multi_target} PRIVATE
        STM32F407xx NX_MULTI_FACTORY_STM32
        _init=nx_multi_fixture_init _fini=nx_multi_fixture_fini)
    # Keep real GPIO methods; only the external cold pin/clock ports are modeled.
    set(multi_gpio "${multi_target}_gpio")
    add_library(${multi_gpio} OBJECT
        "${NEXUS_SOC_SOURCE_${multi_soc}}/drivers/gpio.c")
    target_link_libraries(${multi_gpio} PRIVATE Nexus::IO Nexus::Arch)
    target_include_directories(${multi_gpio} PRIVATE
        "${multi_bundle}" "${NEXUS_SOC_SOURCE_${multi_soc}}"
        "${NEXUS_SOC_SOURCE_${multi_soc}}/private"
        "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu")
    target_compile_options(${multi_gpio} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/multi_controller_factory_model.h"
        "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
    if(TARGET nexus_build_options)
        target_link_libraries(${multi_gpio} PRIVATE
            nexus_build_options nexus_owned_warnings)
    endif()
    target_sources(${multi_target} PRIVATE $<TARGET_OBJECTS:${multi_gpio}>)
    target_sources(${multi_target} PRIVATE
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_lifecycle.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_coordinator.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/system_lifecycle.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart_cold.c")
    include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
    nexus_stm32f4_official(${multi_gpio})
    nexus_stm32f407_uart_hal(${multi_target})
    target_link_options(${multi_target} PRIVATE "-Wl,--gc-sections")
    set(multi_hal "${multi_target}_stm32f407_hal")
    target_compile_definitions(${multi_hal} PRIVATE NX_MULTI_FACTORY_STM32)
    target_compile_options(${multi_hal} BEFORE PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/${multi_model}")
    # CMSIS declarations precede the model, which precedes the profile.
    nexus_use_cmsis_host_model(${multi_hal})
    target_include_directories(${multi_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    target_include_directories(${multi_gpio} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    target_compile_definitions(${multi_gpio} PRIVATE
        STM32F407xx NX_MULTI_FACTORY_STM32
        nx_stm32_gpio_clock_enable=nx_multi_actual_gpio_clock_enable
        nx_stm32_pin_capture=nx_multi_actual_pin_capture
        nx_stm32_pin_configure=nx_multi_actual_pin_configure
        nx_stm32_pin_restore=nx_multi_actual_pin_restore)

    # Exercise legacy single-controller cold failure through the public entry.
    set(single_input "${NEXUS_SOURCE_DIR}/tests/contracts/stm32_assembly/zg-spi.toml")
    set(single_bundle "${CMAKE_CURRENT_BINARY_DIR}/single-spi-factory-${multi_family}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py" --test-support
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${single_input}"
        --output "${single_bundle}" RESULT_VARIABLE single_generated
        OUTPUT_VARIABLE single_output ERROR_VARIABLE single_error)
    if(NOT single_generated EQUAL 0)
        message(FATAL_ERROR "Single-SPI factory rejected: ${single_output}${single_error}")
    endif()
    nexus_google_configuration_inputs("${single_bundle}")
    set(single_target "nexus_${multi_family}_single_spi_factory_test")
    nexus_google_test(${single_target} ${multi_official}
        SOURCES single_spi_factory_test.cpp
        "${single_bundle}/bindings.c"
        "${NEXUS_SOC_SOURCE_${multi_soc}}/drivers/spi.c"
        "${NEXUS_SOC_SOURCE_${multi_soc}}/drivers/platform_cold.c"
        "${NEXUS_SOC_SOURCE_${multi_soc}}/drivers/platform_lifecycle.c"
        "${NEXUS_SOC_SOURCE_${multi_soc}}/drivers/platform_coordinator.c"
        "${NEXUS_SOC_SOURCE_${multi_soc}}/drivers/system_lifecycle.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${single_bundle}" "${NEXUS_SOC_SOURCE_${multi_soc}}"
            "${NEXUS_SOC_SOURCE_${multi_soc}}/private"
            "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu")
    target_sources(${single_target} PRIVATE $<TARGET_OBJECTS:${multi_gpio}>)
    target_compile_options(${single_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/multi_controller_factory_model.h"
        "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
    target_compile_definitions(${single_target} PRIVATE
        _init=nx_single_spi_fixture_init _fini=nx_single_spi_fixture_fini)
    target_include_directories(${single_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    target_compile_definitions(${single_target} PRIVATE
        STM32F407xx NX_MULTI_FACTORY_STM32)
endforeach()
