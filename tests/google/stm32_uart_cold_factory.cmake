# Each exact cold wrapper executes through its actual generated public factory.
foreach(cold_uart_mode irq irq-words irq-blocks dma-tx rx-dma-idle
        rx-dma-continuous dma-duplex dma-duplex-continuous)
    if(cold_uart_mode STREQUAL "irq-words")
        set(cold_uart_input "${NEXUS_SOURCE_DIR}/tests/contracts/io_evolution_assembly/f407-uart9n1.toml")
    else()
        set(cold_uart_input "${NEXUS_SOURCE_DIR}/tests/contracts/uart_rx_compact_assembly/f407-${cold_uart_mode}.toml")
    endif()
    set(cold_uart_bundle "${CMAKE_CURRENT_BINARY_DIR}/f407-uart-cold-${cold_uart_mode}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${cold_uart_input}"
        --output "${cold_uart_bundle}" RESULT_VARIABLE cold_uart_generated
        OUTPUT_VARIABLE cold_uart_output ERROR_VARIABLE cold_uart_error)
    if(NOT cold_uart_generated EQUAL 0)
        message(FATAL_ERROR "UART cold fixture rejected: ${cold_uart_output}${cold_uart_error}")
    endif()
    nexus_google_configuration_inputs("${cold_uart_bundle}")
    set(cold_uart_sources
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/uart_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/platform_cold.c")
    set(cold_uart_rx 0)
    set(cold_uart_tx 0)
    if(cold_uart_mode MATCHES "^(rx-dma|dma-duplex)")
        set(cold_uart_rx 1)
        list(APPEND cold_uart_sources "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/uart_receive.c")
    endif()
    if(cold_uart_mode MATCHES "^(dma-tx|dma-duplex)")
        set(cold_uart_tx 1)
        list(APPEND cold_uart_sources "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/uart_dma.c")
    endif()
    if(cold_uart_mode MATCHES "^dma-duplex")
        list(APPEND cold_uart_sources "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/uart_duplex.c")
    elseif(cold_uart_mode STREQUAL "irq-words")
        list(APPEND cold_uart_sources "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/uart_word.c")
    elseif(cold_uart_mode STREQUAL "irq-blocks")
        list(APPEND cold_uart_sources "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/uart_stream.c")
    endif()
    string(REPLACE "-" "_" cold_uart_label "${cold_uart_mode}")
    set(cold_uart_target "nexus_stm32_uart_${cold_uart_label}_cold_factory_test")
    nexus_google_test(${cold_uart_target} STM32F4_LL
        SOURCES stm32_uart_cold_factory_test.cpp "${cold_uart_bundle}/bindings.c"
            ${cold_uart_sources}
        LIBRARIES Nexus::IO
        INCLUDES "${cold_uart_bundle}"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model"
            "${NEXUS_SOURCE_DIR}/arch/include")
    target_include_directories(${cold_uart_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    target_compile_definitions(${cold_uart_target} PRIVATE STM32F407xx
        NX_COLD_RX=${cold_uart_rx} NX_COLD_TX=${cold_uart_tx}
        _init=nx_uart_cold_fixture_init _fini=nx_uart_cold_fixture_fini)
    if(cold_uart_mode STREQUAL "irq-words")
        target_compile_definitions(${cold_uart_target} PRIVATE NX_COLD_WORDS)
    elseif(cold_uart_mode STREQUAL "irq-blocks")
        target_compile_definitions(${cold_uart_target} PRIVATE NX_COLD_BLOCKS)
    endif()
    target_compile_options(${cold_uart_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_uart_cold_factory_model.h")
endforeach()
