# Real production DMA provider with external MMIO and cache boundaries.
nexus_google_test(nexus_h743_dma_test SOURCES h743_dma_test.cpp
    h743_dma_model.cpp h743_model.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/uart.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/uart_dma.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32h743/private")
target_include_directories(nexus_h743_dma_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_h743_dma_test PRIVATE STM32H743xx)
target_compile_options(nexus_h743_dma_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_dma_model.h")

# The ordinary generator supplies the tested face, IRQs and clock/cache leases.
set(h7_dma_factory_input
    "${NEXUS_SOURCE_DIR}/tests/contracts/h743_assembly/dma-cache.toml")
set(h7_dma_factory_bundle "${CMAKE_CURRENT_BINARY_DIR}/h743-dma-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${h7_dma_factory_input}"
    --output "${h7_dma_factory_bundle}" RESULT_VARIABLE h7_dma_generated
    OUTPUT_VARIABLE h7_dma_output ERROR_VARIABLE h7_dma_error)
if(NOT h7_dma_generated EQUAL 0)
    message(FATAL_ERROR "H743 DMA fixture rejected: ${h7_dma_output}${h7_dma_error}")
endif()
file(READ "${h7_dma_factory_bundle}/input_paths.json" h7_dma_inputs)
string(JSON h7_dma_input_count LENGTH "${h7_dma_inputs}")
math(EXPR h7_dma_last_input "${h7_dma_input_count} - 1")
foreach(index RANGE 0 ${h7_dma_last_input})
    string(JSON h7_dma_input_name MEMBER "${h7_dma_inputs}" ${index})
    string(JSON h7_dma_input_path GET "${h7_dma_inputs}" "${h7_dma_input_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${h7_dma_input_path}")
endforeach()
nexus_google_test(nexus_h743_dma_factory_test SOURCES h743_dma_factory_test.cpp
    "${h7_dma_factory_bundle}/bindings.c" h743_dma_model.cpp h743_model.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/gpio.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/uart.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/uart_dma.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${h7_dma_factory_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/private")
target_include_directories(nexus_h743_dma_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_h743_dma_factory_test PRIVATE STM32H743xx
    _init=nx_h743_dma_fixture_init _fini=nx_h743_dma_fixture_fini)
target_compile_options(nexus_h743_dma_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_dma_factory_model.h")

nexus_google_test(nexus_h743_receive_test SOURCES h743_receive_test.cpp
    h743_dma_model.cpp h743_model.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/uart.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/uart_receive.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32h743/private")
target_include_directories(nexus_h743_receive_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_h743_receive_test PRIVATE STM32H743xx)
target_compile_options(nexus_h743_receive_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_dma_model.h")

set(h7_receive_input
    "${NEXUS_SOURCE_DIR}/tests/contracts/h743_assembly/rx-cache.toml")
set(h7_receive_bundle "${CMAKE_CURRENT_BINARY_DIR}/h743-receive-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${h7_receive_input}"
    --output "${h7_receive_bundle}" RESULT_VARIABLE h7_receive_generated
    OUTPUT_VARIABLE h7_receive_output ERROR_VARIABLE h7_receive_error)
if(NOT h7_receive_generated EQUAL 0)
    message(FATAL_ERROR "H743 RX fixture rejected: ${h7_receive_output}${h7_receive_error}")
endif()
file(READ "${h7_receive_bundle}/input_paths.json" h7_receive_inputs)
string(JSON h7_receive_input_count LENGTH "${h7_receive_inputs}")
math(EXPR h7_receive_last_input "${h7_receive_input_count} - 1")
foreach(index RANGE 0 ${h7_receive_last_input})
    string(JSON h7_receive_input_name MEMBER "${h7_receive_inputs}" ${index})
    string(JSON h7_receive_input_path GET "${h7_receive_inputs}" "${h7_receive_input_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${h7_receive_input_path}")
endforeach()
nexus_google_test(nexus_h743_receive_factory_test
    SOURCES h743_receive_factory_test.cpp
    "${h7_receive_bundle}/bindings.c" h743_dma_model.cpp h743_model.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/gpio.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/uart.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/uart_receive.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${h7_receive_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/private")
target_include_directories(nexus_h743_receive_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_h743_receive_factory_test PRIVATE STM32H743xx
    _init=nx_h743_receive_fixture_init _fini=nx_h743_receive_fixture_fini)
target_compile_options(nexus_h743_receive_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_dma_factory_model.h")
