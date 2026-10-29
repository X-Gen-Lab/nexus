# Normal maintained TOML inputs; no copied controller/factory implementation.
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
foreach(long_family g474 h743 u575)
    set(long_soc "stm32${long_family}")
    set(long_target "nexus_${long_family}_i2c_long_factory_test")
    set(long_input "${NEXUS_SOURCE_DIR}/tests/contracts/io_evolution_assembly/${long_family}-i2c-long-timing.toml")
    set(long_bundle "${CMAKE_CURRENT_BINARY_DIR}/${long_family}-i2c-long-factory")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${long_input}"
        --output "${long_bundle}" RESULT_VARIABLE long_generated
        OUTPUT_VARIABLE long_output ERROR_VARIABLE long_error)
    if(NOT long_generated EQUAL 0)
        message(FATAL_ERROR "Long I2C factory rejected: ${long_output}${long_error}")
    endif()
    nexus_google_configuration_inputs("${long_bundle}")
    file(READ "${long_bundle}/resolved.json" long_resolved)
    string(JSON long_controller_count LENGTH "${long_resolved}" controllers)
    math(EXPR long_last_controller "${long_controller_count} - 1")
    set(long_timing_count 0)
    foreach(index RANGE 0 ${long_last_controller})
        string(JSON long_kind GET "${long_resolved}" controllers ${index} kind)
        if(long_kind STREQUAL "i2c")
            string(JSON long_timing GET "${long_resolved}" controllers ${index} timing_resolution timingr)
            math(EXPR long_timing_count "${long_timing_count} + 1")
        endif()
    endforeach()
    if(NOT long_timing_count EQUAL 1)
        message(FATAL_ERROR "The maintained long fixture must select exactly one timed I2C")
    endif()
    set(long_sources "${NEXUS_SOC_SOURCE_${long_soc}}/system.c"
        "${NEXUS_SOC_SOURCE_${long_soc}}/drivers/gpio.c"
        "${NEXUS_SOC_SOURCE_${long_soc}}/drivers/i2c.c"
        "${NEXUS_SOC_SOURCE_${long_soc}}/drivers/adc.c"
        "${NEXUS_SOC_SOURCE_${long_soc}}/drivers/flash.c")
    if(long_family STREQUAL "h743" OR long_family STREQUAL "u575")
        list(APPEND long_sources "${NEXUS_SOC_SOURCE_${long_soc}}/platform_cold.c")
    endif()
    if(long_family STREQUAL "g474")
        list(APPEND long_sources "${NEXUS_SOURCE_DIR}/tests/contracts/g474_model.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32g474/drivers/platform_cold.c")
        set(long_official g4)
        set(long_part STM32G474xx)
        set(long_define NX_I2C_LONG_G474)
    elseif(long_family STREQUAL "h743")
        list(APPEND long_sources h743_model.cpp h743_flash_image_model.cpp
            h743_flash_fixture.cpp h743_flash_fixture_generated.cpp
            h743_flash_ram_closure_model.cpp h743_flash_mpu_model.cpp
            h743_flash_ram_observer.cpp ${h743_flash_ram_production_sources}
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/flash_execution.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/spi.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/spi_wire.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/pwm.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/exti.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/watchdog.c")
        set(long_official h7)
        set(long_part STM32H743xx)
        set(long_define NX_I2C_LONG_H743)
    else()
        list(APPEND long_sources u575_model.cpp u575_devices_model.cpp
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/spi.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575/drivers/exti.c")
        set(long_official u5)
        set(long_part STM32U575xx)
        set(long_define NX_I2C_LONG_U575)
    endif()
    nexus_google_test(${long_target}
        SOURCES i2c_v2_long_factory_test.cpp i2c_v2_long_factory_model.cpp
            "${long_bundle}/bindings.c" ${long_sources}
            "${NEXUS_SOURCE_DIR}/drivers/i2c/i2c_v2/i2c_v2.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${long_bundle}" "${NEXUS_SOURCE_DIR}/tests/contracts"
            "${NEXUS_SOC_SOURCE_${long_soc}}/private"
            "${NEXUS_SOURCE_DIR}/drivers/i2c/i2c_v2")
    nexus_stm32_official(${long_target} ${long_official})
    target_include_directories(${long_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_${long_official}/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    nexus_use_cmsis_core(${long_target})
    target_compile_definitions(${long_target} PRIVATE ${long_part} ${long_define}
        NX_I2C_LONG_EXPECTED_TIMING=${long_timing}U
        _init=nx_${long_family}_i2c_long_fixture_init
        _fini=nx_${long_family}_i2c_long_fixture_fini)
    target_compile_options(${long_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/i2c_v2_long_factory_model.h")
    if(long_family STREQUAL "h743")
        target_link_options(${long_target} PRIVATE
            "-Wl,--wrap=nx_stm32h743_flash_ram_initialize")
    endif()
endforeach()
