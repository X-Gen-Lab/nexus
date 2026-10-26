# Actual owned G474 startup/providers against external register boundaries.
foreach(g4_suite system provider)
    set(g4_target "nexus_g474_${g4_suite}_test")
    nexus_google_test(${g4_target}
        SOURCES "g474_${g4_suite}_test.cpp"
            "${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/system.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/gpio.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/uart.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts")
    target_include_directories(${g4_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    nexus_use_cmsis_core(${g4_target})
    target_compile_definitions(${g4_target} PRIVATE STM32G474xx)
    target_compile_options(${g4_target} PRIVATE
        "-include${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.h")
endforeach()

# The real V2 protocol engine with external register handshakes only.
nexus_google_test(nexus_g474_i2c_v2_test SOURCES g474_i2c_v2_test.cpp
    "${NEXUS_SOURCE_DIR}/drivers/i2c/i2c_v2/i2c_v2.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/i2c.c"
    "${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/system.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/private"
        "${NEXUS_SOURCE_DIR}/drivers/i2c/i2c_v2"
        "${NEXUS_SOURCE_DIR}/tests/contracts")
target_include_directories(nexus_g474_i2c_v2_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_g474_i2c_v2_test)
target_compile_definitions(nexus_g474_i2c_v2_test PRIVATE STM32G474xx)
target_compile_options(nexus_g474_i2c_v2_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/g474_i2c_v2_model.h")

nexus_google_test(nexus_g474_acquisition_test SOURCES g474_acquisition_test.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/adc.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/adc_stream.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/flash.c"
    "${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/system.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts")
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
nexus_stm32_official(nexus_g474_acquisition_test g4)
target_include_directories(nexus_g474_acquisition_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_g474_acquisition_test)
target_compile_definitions(nexus_g474_acquisition_test PRIVATE STM32G474xx)
target_compile_options(nexus_g474_acquisition_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/g474_acquisition_model.h")

# Actual typed factory owns the cold ADC/TIM6/DMA/I2C/Flash composition.
set(g4_acquisition_input
    "${NEXUS_SOURCE_DIR}/tests/contracts/g474_assembly/acquisition-dma.toml")
set(g4_acquisition_bundle "${CMAKE_CURRENT_BINARY_DIR}/g474-acquisition-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${g4_acquisition_input}"
    --output "${g4_acquisition_bundle}" RESULT_VARIABLE g4_acquisition_result
    OUTPUT_VARIABLE g4_acquisition_output ERROR_VARIABLE g4_acquisition_error)
if(NOT g4_acquisition_result EQUAL 0)
    message(FATAL_ERROR
        "G474 acquisition fixture rejected: ${g4_acquisition_output}${g4_acquisition_error}")
endif()
file(READ "${g4_acquisition_bundle}/input_paths.json" g4_acquisition_inputs)
string(JSON g4_acquisition_count LENGTH "${g4_acquisition_inputs}")
math(EXPR g4_acquisition_last "${g4_acquisition_count} - 1")
foreach(index RANGE 0 ${g4_acquisition_last})
    string(JSON g4_acquisition_name MEMBER "${g4_acquisition_inputs}" ${index})
    string(JSON g4_acquisition_path GET "${g4_acquisition_inputs}"
        "${g4_acquisition_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${g4_acquisition_path}")
endforeach()
nexus_google_test(nexus_g474_acquisition_factory_test
    SOURCES g474_acquisition_factory_test.cpp
        "${g4_acquisition_bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/adc.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/adc_stream.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/i2c.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/flash.c"
        "${NEXUS_SOURCE_DIR}/drivers/i2c/i2c_v2/i2c_v2.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${g4_acquisition_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/private"
        "${NEXUS_SOURCE_DIR}/drivers/i2c/i2c_v2"
        "${NEXUS_SOURCE_DIR}/tests/contracts")
nexus_stm32_official(nexus_g474_acquisition_factory_test g4)
target_include_directories(nexus_g474_acquisition_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_g474_acquisition_factory_test)
target_compile_definitions(nexus_g474_acquisition_factory_test PRIVATE STM32G474xx
    _init=nx_g474_acquisition_factory_init _fini=nx_g474_acquisition_factory_fini)
target_compile_options(nexus_g474_acquisition_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/g474_acquisition_factory_model.h")
