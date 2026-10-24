# Actual U575 providers; CPU/MMIO/NVIC leaves are the only modeled boundaries.
nexus_google_test(nexus_u575_devices_test
    SOURCES u575_devices_test.cpp u575_model.cpp u575_devices_model.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/pwm.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/spi.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/exti.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/adc.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/flash.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/i2c.c"
        "${NEXUS_SOURCE_DIR}/soc/common/i2c_v2.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/system.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/private"
        "${NEXUS_SOURCE_DIR}/tests/google")
target_include_directories(nexus_u575_devices_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_u575_devices_test PRIVATE STM32U575xx)
target_compile_options(nexus_u575_devices_test PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/google/u575_devices_model.h")
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
nexus_stm32_official(nexus_u575_devices_test u5)

set(u5_peripheral_bundle "${CMAKE_CURRENT_BINARY_DIR}/u575-peripherals")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}"
    --assembly "${NEXUS_SOURCE_DIR}/tests/contracts/u575_assembly/peripherals.toml"
    --output "${u5_peripheral_bundle}" RESULT_VARIABLE u5_peripheral_generated
    OUTPUT_VARIABLE u5_peripheral_output ERROR_VARIABLE u5_peripheral_error)
if(NOT u5_peripheral_generated EQUAL 0)
    message(FATAL_ERROR
        "U575 peripheral assembly rejected: ${u5_peripheral_output}${u5_peripheral_error}")
endif()
file(READ "${u5_peripheral_bundle}/input_paths.json" u5_peripheral_inputs)
string(JSON u5_peripheral_count LENGTH "${u5_peripheral_inputs}")
math(EXPR u5_peripheral_last "${u5_peripheral_count} - 1")
foreach(index RANGE 0 ${u5_peripheral_last})
    string(JSON u5_peripheral_name MEMBER "${u5_peripheral_inputs}" ${index})
    string(JSON u5_peripheral_path GET "${u5_peripheral_inputs}"
        "${u5_peripheral_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${u5_peripheral_path}")
endforeach()
nexus_google_test(nexus_u575_peripheral_factory_test
    SOURCES u575_peripheral_factory_test.cpp u575_model.cpp u575_devices_model.cpp
        "${u5_peripheral_bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/spi.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/i2c.c"
        "${NEXUS_SOURCE_DIR}/soc/common/i2c_v2.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/adc.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/exti.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/flash.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${u5_peripheral_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/private"
        "${NEXUS_SOURCE_DIR}/tests/google")
target_include_directories(nexus_u575_peripheral_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_u575_peripheral_factory_test PRIVATE STM32U575xx
    _init=nx_u575_peripheral_fixture_init _fini=nx_u575_peripheral_fixture_fini)
target_compile_options(nexus_u575_peripheral_factory_test PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/google/u575_devices_model.h")
nexus_stm32_official(nexus_u575_peripheral_factory_test u5)

nexus_google_test(nexus_u575_mainstream_test
    SOURCES u575_mainstream_test.cpp u575_watchdog_test.cpp u575_model.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/watchdog.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/private"
        "${NEXUS_SOURCE_DIR}/tests/google")
target_include_directories(nexus_u575_mainstream_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_u575_mainstream_test PRIVATE STM32U575xx)
target_compile_options(nexus_u575_mainstream_test PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/google/u575_model.h")

set(u5_mainstream_input
    "${NEXUS_SOURCE_DIR}/tests/contracts/u575_assembly/mainstream.toml")
set(u5_mainstream_bundle "${CMAKE_CURRENT_BINARY_DIR}/u575-mainstream")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${u5_mainstream_input}"
    --output "${u5_mainstream_bundle}" RESULT_VARIABLE u5_mainstream_generated
    OUTPUT_VARIABLE u5_mainstream_output ERROR_VARIABLE u5_mainstream_error)
if(NOT u5_mainstream_generated EQUAL 0)
    message(FATAL_ERROR
        "U575 mainstream rejected: ${u5_mainstream_output}${u5_mainstream_error}")
endif()
file(READ "${u5_mainstream_bundle}/input_paths.json" u5_mainstream_inputs)
string(JSON u5_mainstream_count LENGTH "${u5_mainstream_inputs}")
math(EXPR u5_mainstream_last "${u5_mainstream_count} - 1")
foreach(index RANGE 0 ${u5_mainstream_last})
    string(JSON u5_mainstream_name MEMBER "${u5_mainstream_inputs}" ${index})
    string(JSON u5_mainstream_path GET "${u5_mainstream_inputs}"
        "${u5_mainstream_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${u5_mainstream_path}")
endforeach()
nexus_google_test(nexus_u575_mainstream_factory_test
    SOURCES u575_mainstream_factory_test.cpp u575_model.cpp
        "${u5_mainstream_bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/watchdog.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${u5_mainstream_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/private"
        "${NEXUS_SOURCE_DIR}/tests/google")
target_include_directories(nexus_u575_mainstream_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_u5/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_u575_mainstream_factory_test PRIVATE STM32U575xx
    _init=nx_u575_fixture_init _fini=nx_u575_fixture_fini)
target_compile_options(nexus_u575_mainstream_factory_test PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/google/u575_mainstream_model.h")
