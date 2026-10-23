# One maintained generated assembly and the actual H743 lifecycle providers.
set(runtime_bundle "${CMAKE_CURRENT_BINARY_DIR}/h743-runtime-clock")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly
    "${NEXUS_SOURCE_DIR}/tools/configure/assemblies/h743-spi-dma-runtime-clock.toml"
    --output "${runtime_bundle}" RESULT_VARIABLE runtime_generated
    OUTPUT_VARIABLE runtime_stdout ERROR_VARIABLE runtime_stderr)
if(NOT runtime_generated EQUAL 0)
    message(FATAL_ERROR "H743 runtime assembly rejected: ${runtime_stdout}${runtime_stderr}")
endif()
file(READ "${runtime_bundle}/input_paths.json" runtime_inputs)
string(JSON runtime_count LENGTH "${runtime_inputs}")
math(EXPR runtime_last "${runtime_count} - 1")
foreach(index RANGE 0 ${runtime_last})
    string(JSON name MEMBER "${runtime_inputs}" ${index})
    string(JSON path GET "${runtime_inputs}" "${name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
endforeach()
nexus_google_test(nexus_h743_runtime_clock_test SOURCES
    h743_runtime_clock_test.cpp h743_runtime_clock_model.cpp
    h743_spi_dma_factory_model.cpp h743_spi_dma_model.cpp h743_model.cpp
    "${runtime_bundle}/bindings.c"
    "${NEXUS_SOURCE_DIR}/components/clock_domain/src/clock_domain.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/gpio.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/spi_wire.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/spi_dma.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${runtime_bundle}" "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/private"
        "${NEXUS_SOURCE_DIR}/components/clock_domain/include"
        "${NEXUS_SOURCE_DIR}/components/clock_domain/private")
target_include_directories(nexus_h743_runtime_clock_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_h743_runtime_clock_test PRIVATE STM32H743xx
    _init=nx_h743_runtime_init _fini=nx_h743_runtime_fini)
target_compile_options(nexus_h743_runtime_clock_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_runtime_clock_model.h")
