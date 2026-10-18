set(h743_high_model_sources h743_model.cpp h743_clock_model.cpp
    h743_high_profile_model.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/system.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/clock.c")
nexus_google_test(nexus_h743_high_system_test
    SOURCES h743_high_system_test.cpp ${h743_high_model_sources}
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32h743/private")
nexus_google_test(nexus_h743_high_kernel_test
    SOURCES h743_high_kernel_test.cpp ${h743_high_model_sources}
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/spi.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/spi_wire.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/i2c.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/pwm.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/adc.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/flash.c"
        "${NEXUS_SOURCE_DIR}/soc/common/i2c_v2.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32h743/private"
        "${NEXUS_SOURCE_DIR}/soc/common")
foreach(h743_high_target nexus_h743_high_system_test nexus_h743_high_kernel_test)
    target_include_directories(${h743_high_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32h7_hal/Inc"
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
        "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
    target_compile_definitions(${h743_high_target} PRIVATE STM32H743xx)
    target_compile_options(${h743_high_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_high_profile_model.h")
endforeach()

# Exercise normal TOML generation, static factory and real lifecycle owners.
set(h7_high_input "${NEXUS_SOURCE_DIR}/cmake/package/verification/io_evolution/h743-high-peripherals.toml")
set(h7_high_bundle "${CMAKE_CURRENT_BINARY_DIR}/h743-high-factory")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${h7_high_input}"
    --output "${h7_high_bundle}" RESULT_VARIABLE h7_high_generated
    OUTPUT_VARIABLE h7_high_output ERROR_VARIABLE h7_high_error)
if(NOT h7_high_generated EQUAL 0)
    message(FATAL_ERROR "H743 high factory rejected: ${h7_high_output}${h7_high_error}")
endif()
file(READ "${h7_high_bundle}/input_paths.json" h7_high_inputs)
string(JSON h7_high_input_count LENGTH "${h7_high_inputs}")
math(EXPR h7_high_last_input "${h7_high_input_count} - 1")
foreach(index RANGE 0 ${h7_high_last_input})
    string(JSON h7_high_input_name MEMBER "${h7_high_inputs}" ${index})
    string(JSON h7_high_input_path GET "${h7_high_inputs}" "${h7_high_input_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${h7_high_input_path}")
endforeach()
nexus_google_test(nexus_h743_high_factory_test
    SOURCES h743_high_factory_test.cpp "${h7_high_bundle}/bindings.c"
        ${h743_high_model_sources}
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/gpio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/spi.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/spi_wire.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/pwm.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/adc.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/flash.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/i2c.c"
        "${NEXUS_SOURCE_DIR}/soc/common/i2c_v2.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${h7_high_bundle}"
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/private"
        "${NEXUS_SOURCE_DIR}/soc/common")
target_include_directories(nexus_h743_high_factory_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/stm32h7_hal/Inc"
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_h743_high_factory_test PRIVATE STM32H743xx
    _init=nx_h743_high_fixture_init _fini=nx_h743_high_fixture_fini)
target_compile_options(nexus_h743_high_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_high_factory_model.h")

# Keep linked-image admission in both isolated and generated high-profile paths.
foreach(h743_high_flash_target nexus_h743_high_kernel_test
        nexus_h743_high_factory_test)
    target_sources(${h743_high_flash_target} PRIVATE h743_flash_image_model.cpp
        h743_flash_fixture.cpp h743_flash_ram_closure_model.cpp
        h743_flash_mpu_model.cpp ${h743_flash_ram_production_sources}
        "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/flash_execution.c")
    target_compile_options(${h743_high_flash_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_flash_image_model.h")
endforeach()

target_compile_options(nexus_h743_high_kernel_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_flash_fixture_high_model.h")
target_compile_options(nexus_h743_high_factory_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_flash_ram_high_model.h")
target_sources(nexus_h743_high_factory_test PRIVATE
    h743_flash_fixture_generated.cpp)
target_link_options(nexus_h743_high_factory_test PRIVATE
    "-Wl,--wrap=nx_stm32h743_flash_ram_initialize")
