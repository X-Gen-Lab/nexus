# Actual GD32F303 code; only external register and vector boundaries are modeled.
foreach(gd303_suite system provider watchdog spi exti)
set(gd303_target nexus_gd32f303_${gd303_suite}_test)
nexus_google_test(${gd303_target}
    SOURCES gd32f303_${gd303_suite}_test.cpp
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32f303_model/model.c"
        "${NEXUS_SOURCE_DIR}/soc/gd32f303/system.c"
        "${NEXUS_SOURCE_DIR}/soc/gd32f303/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/gd32f303/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/gd32f303/drivers/watchdog.c"
        "${NEXUS_SOURCE_DIR}/soc/gd32f303/drivers/spi.c"
        "${NEXUS_SOURCE_DIR}/soc/gd32f303/drivers/exti.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/gd32f303/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32f303_model")
target_include_directories(${gd303_target} SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f30x/Firmware/CMSIS/GD/GD32F30x/Include"
    "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f30x/Firmware/GD32F30x_standard_peripheral/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(${gd303_target} PRIVATE GD32F30X_HD)
target_compile_options(${gd303_target} PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/contracts/gd32f303_model/model.h")
endforeach()

# Generate and compile a real authored GD303 assembly; no hand-built factory.
set(gd303_factory_bundle "${CMAKE_CURRENT_BINARY_DIR}/gd303-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}"
    --assembly "${NEXUS_SOURCE_DIR}/tools/configure/assemblies/gd32-f303-baremetal-io.toml"
    --output "${gd303_factory_bundle}"
    RESULT_VARIABLE gd303_generated
    OUTPUT_VARIABLE gd303_output ERROR_VARIABLE gd303_error)
if(NOT gd303_generated EQUAL 0)
    message(FATAL_ERROR "GD303 factory fixture rejected: ${gd303_output}${gd303_error}")
endif()
file(READ "${gd303_factory_bundle}/input_paths.json" gd303_inputs)
string(JSON gd303_input_count LENGTH "${gd303_inputs}")
math(EXPR gd303_last_input "${gd303_input_count} - 1")
foreach(index RANGE 0 ${gd303_last_input})
    string(JSON gd303_input_name MEMBER "${gd303_inputs}" ${index})
    string(JSON gd303_input_path GET "${gd303_inputs}" "${gd303_input_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${gd303_input_path}")
endforeach()
nexus_google_test(nexus_gd32f303_factory_test SOURCES gd32f303_factory_test.cpp
    "${gd303_factory_bundle}/bindings.c"
    "${NEXUS_SOURCE_DIR}/tests/contracts/gd32f303_model/model.c"
    "${NEXUS_SOURCE_DIR}/soc/gd32f303/system.c"
    "${NEXUS_SOURCE_DIR}/soc/gd32f303/drivers/gpio.c"
    "${NEXUS_SOURCE_DIR}/soc/gd32f303/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/gd32f303/drivers/watchdog.c"
        "${NEXUS_SOURCE_DIR}/soc/gd32f303/drivers/spi.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${gd303_factory_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/gd32f303/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32f303_model")
target_include_directories(nexus_gd32f303_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f30x/Firmware/CMSIS/GD/GD32F30x/Include"
    "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f30x/Firmware/GD32F30x_standard_peripheral/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_gd32f303_factory_test PRIVATE GD32F30X_HD
    _init=nx_gd303_fixture_init _fini=nx_gd303_fixture_fini)
target_compile_options(nexus_gd32f303_factory_test PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/contracts/gd32f303_model/model.h")

# Generate and compile a real authored GD303 assembly; no hand-built factory.
set(gd303_spi_factory_bundle "${CMAKE_CURRENT_BINARY_DIR}/gd303-spi-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}"
    --assembly "${NEXUS_SOURCE_DIR}/tools/configure/assemblies/gd32-f303-baremetal-spi-watchdog.toml"
    --output "${gd303_spi_factory_bundle}"
    RESULT_VARIABLE gd303_generated
    OUTPUT_VARIABLE gd303_output ERROR_VARIABLE gd303_error)
if(NOT gd303_generated EQUAL 0)
    message(FATAL_ERROR "GD303 factory fixture rejected: ${gd303_output}${gd303_error}")
endif()
file(READ "${gd303_spi_factory_bundle}/input_paths.json" gd303_inputs)
string(JSON gd303_input_count LENGTH "${gd303_inputs}")
math(EXPR gd303_last_input "${gd303_input_count} - 1")
foreach(index RANGE 0 ${gd303_last_input})
    string(JSON gd303_input_name MEMBER "${gd303_inputs}" ${index})
    string(JSON gd303_input_path GET "${gd303_inputs}" "${gd303_input_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${gd303_input_path}")
endforeach()
nexus_google_test(nexus_gd32f303_spi_factory_test SOURCES gd32f303_spi_factory_test.cpp
    "${gd303_spi_factory_bundle}/bindings.c"
    "${NEXUS_SOURCE_DIR}/tests/contracts/gd32f303_model/model.c"
    "${NEXUS_SOURCE_DIR}/soc/gd32f303/system.c"
    "${NEXUS_SOURCE_DIR}/soc/gd32f303/drivers/gpio.c"
    "${NEXUS_SOURCE_DIR}/soc/gd32f303/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/gd32f303/drivers/watchdog.c"
        "${NEXUS_SOURCE_DIR}/soc/gd32f303/drivers/spi.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${gd303_spi_factory_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/gd32f303/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32f303_model")
target_include_directories(nexus_gd32f303_spi_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f30x/Firmware/CMSIS/GD/GD32F30x/Include"
    "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f30x/Firmware/GD32F30x_standard_peripheral/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_gd32f303_spi_factory_test PRIVATE GD32F30X_HD
    _init=nx_gd303_fixture_init _fini=nx_gd303_fixture_fini)
target_compile_options(nexus_gd32f303_spi_factory_test PRIVATE
    "-include${NEXUS_SOURCE_DIR}/tests/contracts/gd32f303_model/model.h")
