# One real G474 assembly; only external CPU and MMIO leaves are modeled.
set(g4_factory_input "${NEXUS_SOURCE_DIR}/tests/contracts/g474_assembly/mainstream.toml")
set(g4_factory_bundle "${CMAKE_CURRENT_BINARY_DIR}/g474-mainstream")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${NEXUS_SOURCE_DIR}/tools/configure/providers/stm32g474/__init__.py"
    "${NEXUS_SOURCE_DIR}/tools/configure/providers/stm32g474/bindings.py"
    "${NEXUS_SOURCE_DIR}/tools/configure/providers/stm32g474/emission.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${g4_factory_input}"
    --output "${g4_factory_bundle}" RESULT_VARIABLE g4_generated
    OUTPUT_VARIABLE g4_output ERROR_VARIABLE g4_error)
if(NOT g4_generated EQUAL 0)
    message(FATAL_ERROR "G474 factory fixture rejected: ${g4_output}${g4_error}")
endif()
file(READ "${g4_factory_bundle}/input_paths.json" g4_inputs)
string(JSON g4_input_count LENGTH "${g4_inputs}")
math(EXPR g4_last_input "${g4_input_count} - 1")
foreach(index RANGE 0 ${g4_last_input})
    string(JSON g4_input_name MEMBER "${g4_inputs}" ${index})
    string(JSON g4_input_path GET "${g4_inputs}" "${g4_input_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${g4_input_path}")
endforeach()
nexus_google_test(nexus_g474_mainstream_test SOURCES g474_mainstream_test.cpp
    "${g4_factory_bundle}/bindings.c"
    "${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/gpio.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/uart.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${g4_factory_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts")
target_include_directories(nexus_g474_mainstream_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_g474_mainstream_test PRIVATE STM32G474xx
    _init=nx_g474_mainstream_init _fini=nx_g474_mainstream_fini)
target_compile_options(nexus_g474_mainstream_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/g474_mainstream_model.h")

# Actual irreversible watchdog implementation, external MMIO only modeled.
nexus_google_test(nexus_g474_watchdog_test SOURCES g474_watchdog_test.cpp
    "${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/watchdog.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts")
target_include_directories(nexus_g474_watchdog_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_g474_watchdog_test PRIVATE STM32G474xx)
target_compile_options(nexus_g474_watchdog_test PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.h")

nexus_google_test(nexus_g474_spi_test SOURCES g474_spi_test.cpp
    "${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/gpio.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/spi.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts")
target_include_directories(nexus_g474_spi_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_g474_spi_test PRIVATE STM32G474xx)
target_compile_options(nexus_g474_spi_test PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.h")

# One real G474 assembly; only external CPU and MMIO leaves are modeled.
set(g4_factory_input "${NEXUS_SOURCE_DIR}/tests/contracts/g474_assembly/spi.toml")
set(g4_factory_bundle "${CMAKE_CURRENT_BINARY_DIR}/g474-peripheral-factory")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${NEXUS_SOURCE_DIR}/tools/configure/providers/stm32g474/__init__.py"
    "${NEXUS_SOURCE_DIR}/tools/configure/providers/stm32g474/bindings.py"
    "${NEXUS_SOURCE_DIR}/tools/configure/providers/stm32g474/emission.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${g4_factory_input}"
    --output "${g4_factory_bundle}" RESULT_VARIABLE g4_generated
    OUTPUT_VARIABLE g4_output ERROR_VARIABLE g4_error)
if(NOT g4_generated EQUAL 0)
    message(FATAL_ERROR "G474 factory fixture rejected: ${g4_output}${g4_error}")
endif()
file(READ "${g4_factory_bundle}/input_paths.json" g4_inputs)
string(JSON g4_input_count LENGTH "${g4_inputs}")
math(EXPR g4_last_input "${g4_input_count} - 1")
foreach(index RANGE 0 ${g4_last_input})
    string(JSON g4_input_name MEMBER "${g4_inputs}" ${index})
    string(JSON g4_input_path GET "${g4_inputs}" "${g4_input_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${g4_input_path}")
endforeach()
nexus_google_test(nexus_g474_peripheral_factory_test SOURCES g474_peripheral_factory_test.cpp
    "${g4_factory_bundle}/bindings.c"
    "${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/gpio.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/spi.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/watchdog.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${g4_factory_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts")
target_include_directories(nexus_g474_peripheral_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_g474_peripheral_factory_test PRIVATE STM32G474xx
    _init=nx_g474_peripheral_init _fini=nx_g474_peripheral_fini)
target_compile_options(nexus_g474_peripheral_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/g474_peripheral_factory_model.h")

nexus_google_test(nexus_g474_pwm_test SOURCES g474_pwm_test.cpp
    "${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/gpio.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/pwm.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts")
target_include_directories(nexus_g474_pwm_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_g474_pwm_test PRIVATE STM32G474xx)
target_compile_options(nexus_g474_pwm_test PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.h")

# One real G474 assembly; only external CPU and MMIO leaves are modeled.
set(g4_factory_input "${NEXUS_SOURCE_DIR}/tests/contracts/g474_assembly/pwm.toml")
set(g4_factory_bundle "${CMAKE_CURRENT_BINARY_DIR}/g474-pwm-factory")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${NEXUS_SOURCE_DIR}/tools/configure/providers/stm32g474/__init__.py"
    "${NEXUS_SOURCE_DIR}/tools/configure/providers/stm32g474/bindings.py"
    "${NEXUS_SOURCE_DIR}/tools/configure/providers/stm32g474/emission.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${g4_factory_input}"
    --output "${g4_factory_bundle}" RESULT_VARIABLE g4_generated
    OUTPUT_VARIABLE g4_output ERROR_VARIABLE g4_error)
if(NOT g4_generated EQUAL 0)
    message(FATAL_ERROR "G474 factory fixture rejected: ${g4_output}${g4_error}")
endif()
file(READ "${g4_factory_bundle}/input_paths.json" g4_inputs)
string(JSON g4_input_count LENGTH "${g4_inputs}")
math(EXPR g4_last_input "${g4_input_count} - 1")
foreach(index RANGE 0 ${g4_last_input})
    string(JSON g4_input_name MEMBER "${g4_inputs}" ${index})
    string(JSON g4_input_path GET "${g4_inputs}" "${g4_input_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${g4_input_path}")
endforeach()
nexus_google_test(nexus_g474_pwm_factory_test SOURCES g474_pwm_factory_test.cpp
    "${g4_factory_bundle}/bindings.c"
    "${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/gpio.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/pwm.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${g4_factory_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts")
target_include_directories(nexus_g474_pwm_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_g4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_g474_pwm_factory_test PRIVATE STM32G474xx
    _init=nx_g474_pwm_factory_init _fini=nx_g474_pwm_factory_fini)
target_compile_options(nexus_g474_pwm_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/g474_pwm_factory_model.h")

