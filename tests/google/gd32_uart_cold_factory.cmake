set(cold_gd_sdk "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
add_library(nexus_gd32_uart_cold_vendor OBJECT
    "${cold_gd_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_rcu.c"
    "${cold_gd_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_usart.c"
    "${cold_gd_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_gpio.c")
target_include_directories(nexus_gd32_uart_cold_vendor PRIVATE
    "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/private"
    "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
target_include_directories(nexus_gd32_uart_cold_vendor SYSTEM PRIVATE
    "${cold_gd_sdk}/CMSIS/GD/GD32F4xx/Include"
    "${cold_gd_sdk}/GD32F4xx_standard_peripheral/Include")
target_compile_definitions(nexus_gd32_uart_cold_vendor PRIVATE
    GD32F470 HXTAL_VALUE=25000000)
target_compile_features(nexus_gd32_uart_cold_vendor PRIVATE c_std_11)
target_compile_options(nexus_gd32_uart_cold_vendor PRIVATE -Wno-int-to-pointer-cast)
# Each exact cold wrapper executes through its actual generated public factory.
foreach(cold_uart_mode irq irq-words irq-blocks dma-tx rx-dma-idle
        rx-dma-continuous dma-duplex dma-duplex-continuous)
    if(cold_uart_mode STREQUAL "irq-words")
        set(cold_uart_input "${NEXUS_SOURCE_DIR}/tests/contracts/io_evolution_assembly/gd470-uart9n1.toml")
    else()
        set(cold_uart_input "${NEXUS_SOURCE_DIR}/tests/contracts/uart_rx_compact_assembly/gd470-${cold_uart_mode}.toml")
    endif()
    set(cold_uart_bundle "${CMAKE_CURRENT_BINARY_DIR}/gd470-uart-cold-${cold_uart_mode}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${cold_uart_input}"
        --output "${cold_uart_bundle}" RESULT_VARIABLE cold_uart_generated
        OUTPUT_VARIABLE cold_uart_output ERROR_VARIABLE cold_uart_error)
    if(NOT cold_uart_generated EQUAL 0)
        message(FATAL_ERROR "UART cold fixture rejected: ${cold_uart_output}${cold_uart_error}")
    endif()
    foreach(catalog input_paths configuration_code_paths)
        file(READ "${cold_uart_bundle}/${catalog}.json" cold_uart_inputs)
        string(JSON cold_uart_count LENGTH "${cold_uart_inputs}")
        math(EXPR cold_uart_last "${cold_uart_count} - 1")
        foreach(index RANGE 0 ${cold_uart_last})
            string(JSON label MEMBER "${cold_uart_inputs}" ${index})
            string(JSON input GET "${cold_uart_inputs}" "${label}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input}")
        endforeach()
    endforeach()
    set(cold_uart_sources
        "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/drivers/uart_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/drivers/platform_cold.c")
    set(cold_uart_rx 0)
    set(cold_uart_tx 0)
    if(cold_uart_mode MATCHES "^(rx-dma|dma-duplex)")
        set(cold_uart_rx 1)
        list(APPEND cold_uart_sources "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/drivers/uart_receive.c")
    endif()
    if(cold_uart_mode MATCHES "^(dma-tx|dma-duplex)")
        set(cold_uart_tx 1)
        list(APPEND cold_uart_sources "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/drivers/uart_dma.c")
    endif()
    if(cold_uart_mode MATCHES "^dma-duplex")
        list(APPEND cold_uart_sources "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/drivers/uart_duplex.c")
    elseif(cold_uart_mode STREQUAL "irq-words")
        list(APPEND cold_uart_sources "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/drivers/uart_word.c")
    elseif(cold_uart_mode STREQUAL "irq-blocks")
        list(APPEND cold_uart_sources "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/drivers/uart_stream.c")
    endif()
    string(REPLACE "-" "_" cold_uart_label "${cold_uart_mode}")
    set(cold_uart_target "nexus_gd32_uart_${cold_uart_label}_cold_factory_test")
    nexus_google_test(${cold_uart_target}
        SOURCES "${NEXUS_SOURCE_DIR}/tests/google/gd32_uart_cold_factory_test.cpp" "${cold_uart_bundle}/bindings.c"
            ${cold_uart_sources}
        LIBRARIES Nexus::IO
        INCLUDES "${cold_uart_bundle}"
            "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470"
            "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model"
            "${NEXUS_SOURCE_DIR}/arch/include")
    target_sources(${cold_uart_target} PRIVATE
        $<TARGET_OBJECTS:nexus_gd32_uart_cold_vendor>)
    target_include_directories(${cold_uart_target} SYSTEM PRIVATE
        "${cold_gd_sdk}/CMSIS/GD/GD32F4xx/Include"
        "${cold_gd_sdk}/GD32F4xx_standard_peripheral/Include")
    target_compile_options(${cold_uart_target} PRIVATE
        "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
    target_compile_definitions(${cold_uart_target} PRIVATE GD32F470 HXTAL_VALUE=25000000
        NX_COLD_RX=${cold_uart_rx} NX_COLD_TX=${cold_uart_tx}
        _init=nx_uart_cold_fixture_init _fini=nx_uart_cold_fixture_fini)
    if(cold_uart_mode STREQUAL "irq-words")
        target_compile_definitions(${cold_uart_target} PRIVATE NX_COLD_WORDS)
    elseif(cold_uart_mode STREQUAL "irq-blocks")
        target_compile_definitions(${cold_uart_target} PRIVATE NX_COLD_BLOCKS)
    endif()
    target_compile_options(${cold_uart_target} PRIVATE
        "-include${NEXUS_SOURCE_DIR}/tests/google/gd32_uart_cold_factory_model.h")
endforeach()
