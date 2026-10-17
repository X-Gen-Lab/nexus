# Actual H743 provider sources with real fixed SDK register definitions.
nexus_google_test(nexus_h743_clock_test SOURCES h743_clock_test.cpp
    h743_clock_model.cpp h743_model.cpp "${NEXUS_SOURCE_DIR}/soc/stm32h743/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/clock.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32h743/private")
target_include_directories(nexus_h743_clock_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_h743_clock_test PRIVATE STM32H743xx)
target_compile_options(nexus_h743_clock_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_clock_model.h")

nexus_google_test(nexus_h743_memory_test SOURCES h743_memory_test.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/memory_init.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32h743")
target_compile_definitions(nexus_h743_memory_test PRIVATE NEXUS_H743_MEMORY_MODEL)

foreach(h7_suite system provider)
    set(h7_target "nexus_h743_${h7_suite}_test")
    nexus_google_test(${h7_target} SOURCES "h743_${h7_suite}_test.cpp"
        h743_model.cpp "${NEXUS_SOURCE_DIR}/soc/stm32h743/system.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/uart.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32h743/private")
    target_include_directories(${h7_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
        "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
    target_compile_definitions(${h7_target} PRIVATE STM32H743xx)
    target_compile_options(${h7_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_model.h")
endforeach()

nexus_google_test(nexus_h743_peripheral_test SOURCES h743_peripheral_test.cpp
    h743_model.cpp "${NEXUS_SOURCE_DIR}/soc/stm32h743/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/gpio.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/pwm.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/exti.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/watchdog.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/spi.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32h743/private")
target_include_directories(nexus_h743_peripheral_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/stm32h7_hal/Inc"
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_h743_peripheral_test PRIVATE STM32H743xx)
target_compile_options(nexus_h743_peripheral_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_peripheral_model.h")

nexus_google_test(nexus_h743_conversion_test SOURCES h743_conversion_test.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/adc.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/flash.c"
    h743_model.cpp "${NEXUS_SOURCE_DIR}/soc/stm32h743/system.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32h743/private")
target_include_directories(nexus_h743_conversion_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/stm32h7_hal/Inc"
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_h743_conversion_test PRIVATE STM32H743xx)
target_compile_options(nexus_h743_conversion_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_peripheral_model.h")

# One authored assembly; static factory identity and actual cold leases.
set(h7_factory_input "${NEXUS_SOURCE_DIR}/tests/contracts/h743_assembly/io.toml")
set(h7_factory_bundle "${CMAKE_CURRENT_BINARY_DIR}/h743-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${h7_factory_input}"
    --output "${h7_factory_bundle}" RESULT_VARIABLE h7_generated
    OUTPUT_VARIABLE h7_output ERROR_VARIABLE h7_error)
if(NOT h7_generated EQUAL 0)
    message(FATAL_ERROR "H743 factory fixture rejected: ${h7_output}${h7_error}")
endif()
file(READ "${h7_factory_bundle}/input_paths.json" h7_inputs)
string(JSON h7_input_count LENGTH "${h7_inputs}")
math(EXPR h7_last_input "${h7_input_count} - 1")
foreach(index RANGE 0 ${h7_last_input})
    string(JSON h7_input_name MEMBER "${h7_inputs}" ${index})
    string(JSON h7_input_path GET "${h7_inputs}" "${h7_input_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${h7_input_path}")
endforeach()
nexus_google_test(nexus_h743_factory_test SOURCES h743_factory_test.cpp
    "${h7_factory_bundle}/bindings.c" h743_model.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/gpio.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/uart.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${h7_factory_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/private")
target_include_directories(nexus_h743_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_h743_factory_test PRIVATE STM32H743xx
    _init=nx_h743_fixture_init _fini=nx_h743_fixture_fini)
target_compile_options(nexus_h743_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_factory_model.h")

# One authored assembly; static factory identity and actual cold leases.
set(h7_factory_input "${NEXUS_SOURCE_DIR}/tests/contracts/h743_assembly/peripherals.toml")
set(h7_factory_bundle "${CMAKE_CURRENT_BINARY_DIR}/h743-peripheral-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${h7_factory_input}"
    --output "${h7_factory_bundle}" RESULT_VARIABLE h7_generated
    OUTPUT_VARIABLE h7_output ERROR_VARIABLE h7_error)
if(NOT h7_generated EQUAL 0)
    message(FATAL_ERROR "H743 factory fixture rejected: ${h7_output}${h7_error}")
endif()
file(READ "${h7_factory_bundle}/input_paths.json" h7_inputs)
string(JSON h7_input_count LENGTH "${h7_inputs}")
math(EXPR h7_last_input "${h7_input_count} - 1")
foreach(index RANGE 0 ${h7_last_input})
    string(JSON h7_input_name MEMBER "${h7_inputs}" ${index})
    string(JSON h7_input_path GET "${h7_inputs}" "${h7_input_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${h7_input_path}")
endforeach()
nexus_google_test(nexus_h743_peripheral_factory_test SOURCES h743_peripheral_factory_test.cpp
    "${h7_factory_bundle}/bindings.c" h743_model.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/gpio.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/spi.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/pwm.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/exti.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/adc.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/flash.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/i2c.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/watchdog.c"
    "${NEXUS_SOURCE_DIR}/soc/common/i2c_v2.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${h7_factory_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/private"
        "${NEXUS_SOURCE_DIR}/soc/common")
target_include_directories(nexus_h743_peripheral_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/stm32h7_hal/Inc"
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_h743_peripheral_factory_test PRIVATE STM32H743xx
    _init=nx_h743_fixture_init _fini=nx_h743_fixture_fini)
target_compile_options(nexus_h743_peripheral_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_peripheral_factory_model.h")

# H7 independently binds the shared V2 layout and exact errata policy.
nexus_google_test(nexus_h743_i2c_v2_test SOURCES h743_i2c_v2_test.cpp
    h743_model.cpp "${NEXUS_SOURCE_DIR}/soc/stm32h743/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/i2c.c"
    "${NEXUS_SOURCE_DIR}/soc/common/i2c_v2.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32h743/private"
        "${NEXUS_SOURCE_DIR}/soc/common")
target_include_directories(nexus_h743_i2c_v2_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_h743_i2c_v2_test PRIVATE STM32H743xx)
target_compile_options(nexus_h743_i2c_v2_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_i2c_v2_model.h")

# One external physical image is shared by unit and normal factory consumers.
foreach(h743_flash_target nexus_h743_conversion_test
        nexus_h743_peripheral_factory_test)
    target_sources(${h743_flash_target} PRIVATE h743_flash_image_model.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/flash_execution.c")
    target_compile_options(${h743_flash_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_flash_image_model.h")
endforeach()
