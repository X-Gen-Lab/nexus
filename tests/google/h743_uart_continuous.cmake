# Exact static DBM receive contract; firmware never links GoogleTest.
nexus_google_test(nexus_h743_uart_continuous_test SOURCES
    h743_uart_continuous_test.cpp h743_uart_continuous_model.cpp h743_model.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/uart.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/uart_continuous.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/private")
target_include_directories(nexus_h743_uart_continuous_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_h743_uart_continuous_test)
target_compile_definitions(nexus_h743_uart_continuous_test PRIVATE STM32H743xx)
target_compile_options(nexus_h743_uart_continuous_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_uart_continuous_model.h")

nexus_google_test(nexus_h743_uart_duplex_test SOURCES
    h743_uart_duplex_test.cpp h743_uart_continuous_model.cpp h743_model.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/uart.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/uart_continuous.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/uart_duplex.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/uart_dma.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/private")
target_include_directories(nexus_h743_uart_duplex_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_h743_uart_duplex_test)
target_compile_definitions(nexus_h743_uart_duplex_test PRIVATE STM32H743xx)
target_compile_options(nexus_h743_uart_duplex_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_uart_continuous_model.h")

foreach(mode continuous duplex)
foreach(cache off on)
    set(stem "h743-uart-continuous")
    if(mode STREQUAL "duplex")
        set(stem "h743-uart-duplex-continuous")
    endif()
    set(authored "${NEXUS_SOURCE_DIR}/tools/configure/assemblies/${stem}.toml")
    if(cache STREQUAL "on")
        set(authored "${NEXUS_SOURCE_DIR}/tools/configure/assemblies/${stem}-cache-on.toml")
    endif()
    set(bundle "${CMAKE_CURRENT_BINARY_DIR}/h743-uart-${mode}-${cache}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${authored}"
        --output "${bundle}" RESULT_VARIABLE generated
        OUTPUT_VARIABLE generation_stdout ERROR_VARIABLE generation_stderr)
    if(NOT generated EQUAL 0)
        message(FATAL_ERROR "H743 continuous ${cache} rejected: ${generation_stdout}${generation_stderr}")
    endif()
    file(READ "${bundle}/input_paths.json" inputs)
    string(JSON input_count LENGTH "${inputs}")
    math(EXPR last_input "${input_count} - 1")
    foreach(index RANGE 0 ${last_input})
        string(JSON input_name MEMBER "${inputs}" ${index})
        string(JSON input_path GET "${inputs}" "${input_name}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input_path}")
    endforeach()
    set(target "nexus_h743_uart_${mode}_${cache}_factory_test")
    nexus_google_test(${target} SOURCES h743_uart_continuous_factory_test.cpp
        h743_uart_continuous_model.cpp h743_model.cpp "${bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/uart_continuous.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${bundle}" "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/private")
    if(mode STREQUAL "duplex")
        target_sources(${target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/uart_duplex.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/uart_dma.c")
    endif()
    target_include_directories(${target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    nexus_use_cmsis_core(${target})
    target_compile_definitions(${target} PRIVATE STM32H743xx
        H743_CONTINUOUS_DUPLEX=$<STREQUAL:${mode},duplex>
        H743_CONTINUOUS_CACHE_ON=$<STREQUAL:${cache},on>
        _init=nx_h743_${mode}_${cache}_init
        _fini=nx_h743_${mode}_${cache}_fini)
    target_compile_options(${target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_uart_continuous_model.h")
endforeach()
endforeach()
