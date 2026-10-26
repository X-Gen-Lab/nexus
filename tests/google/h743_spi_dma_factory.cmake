# Normally generated exact SPI Cache modes and independent UART shared owners.
foreach(variant spi-dma spi-dma-cache-on spi-dma-uart-tx spi-dma-uart-rx)
    set(authored
        "${NEXUS_SOURCE_DIR}/tools/configure/assemblies/h743-${variant}.toml")
    set(bundle "${CMAKE_CURRENT_BINARY_DIR}/h743-${variant}-factory")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${authored}"
        --output "${bundle}" RESULT_VARIABLE generated
        OUTPUT_VARIABLE generation_stdout ERROR_VARIABLE generation_stderr)
    if(NOT generated EQUAL 0)
        message(FATAL_ERROR "H743 ${variant} rejected: ${generation_stdout}${generation_stderr}")
    endif()
    file(READ "${bundle}/input_paths.json" inputs)
    string(JSON input_count LENGTH "${inputs}")
    math(EXPR last_input "${input_count} - 1")
    foreach(index RANGE 0 ${last_input})
        string(JSON input_name MEMBER "${inputs}" ${index})
        string(JSON input_path GET "${inputs}" "${input_name}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input_path}")
    endforeach()
    string(REPLACE "-" "_" suffix "${variant}")
    set(target "nexus_h743_${suffix}_factory_test")
    set(uart_mode -1)
    set(cache_on 0)
    if(variant STREQUAL "spi-dma-cache-on")
        set(cache_on 1)
    endif()
    set(uart_sources)
    if(variant STREQUAL "spi-dma-uart-tx")
        set(uart_mode 0)
        set(uart_sources
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/uart.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/uart_dma.c")
    elseif(variant STREQUAL "spi-dma-uart-rx")
        set(uart_mode 1)
        set(uart_sources
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/uart.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/uart_receive.c")
    endif()
    nexus_google_test(${target} SOURCES h743_spi_dma_factory_test.cpp
        h743_spi_dma_factory_model.cpp h743_spi_dma_model.cpp h743_model.cpp
        "${bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/spi_wire.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/spi_dma.c" ${uart_sources}
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${bundle}" "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/private")
    target_include_directories(${target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    nexus_use_cmsis_core(${target})
    target_compile_definitions(${target} PRIVATE STM32H743xx
        D2_UART_MODE=${uart_mode} H743_SPI_DMA_CACHE_ON=${cache_on}
        _init=nx_h743_d2_${suffix}_init _fini=nx_h743_d2_${suffix}_fini)
    target_compile_options(${target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_spi_dma_factory_model.h")
endforeach()
