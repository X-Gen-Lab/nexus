# One exact SDIO owner; the clock variant retains only its crossing contract.
nexus_google_test(nexus_block_port_test SOURCES block_port_test.cpp
    LIBRARIES Nexus::IO)
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32SDIO.cmake")

function(nexus_sdio_contract target)
    nexus_google_test(${target} STM32F4_LL
        SOURCES stm32_sdio_block_test.cpp
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/sdio.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_cold.c"
        LIBRARIES Nexus::IO
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
            "${NEXUS_SOURCE_DIR}/arch/include"
            "${NEXUS_SOURCE_DIR}/tests/google"
            "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu")
    target_include_directories(${target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
    target_compile_definitions(${target} PRIVATE STM32F407xx)
    set(sdio_model "${CMAKE_CURRENT_SOURCE_DIR}/stm32_sdio_block_model.h")
    target_compile_options(${target} PRIVATE "-include${sdio_model}")
    nexus_stm32f407_sdio_ll(${target})
    set(vendor "${target}_sdio_ll")
    target_include_directories(${vendor} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_options(${vendor} BEFORE PRIVATE "-include${sdio_model}")
    # Keep pinned target-address diagnostics visible in the host-only TU.
    target_compile_options(${vendor} PRIVATE
        -Wno-error=pointer-to-int-cast -Wno-error=int-to-pointer-cast)
    nexus_use_cmsis_host_model(${vendor})
    target_link_options(${target} PRIVATE -Wl,--gc-sections)
endfunction()

nexus_sdio_contract(nexus_stm32_sdio_block_test)

set(sdio_clock_input
    "${NEXUS_SOURCE_DIR}/tests/contracts/sdio_clock/low-pclk.toml")
set(sdio_clock_bundle "${CMAKE_CURRENT_BINARY_DIR}/sdio-low-clock")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py" --test-support
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${sdio_clock_input}"
    --output "${sdio_clock_bundle}" RESULT_VARIABLE sdio_clock_result
    OUTPUT_VARIABLE sdio_clock_output ERROR_VARIABLE sdio_clock_error)
if(NOT sdio_clock_result EQUAL 0)
    message(FATAL_ERROR
        "SDIO clock fixture rejected: ${sdio_clock_output}${sdio_clock_error}")
endif()
nexus_google_configuration_inputs("${sdio_clock_bundle}")
set(sdio_clock_target nexus_stm32_sdio_low_clock_test)
nexus_sdio_contract(${sdio_clock_target})
target_compile_definitions(${sdio_clock_target} PRIVATE
    NX_SDIO_LOW_CLOCK_ONLY=1)
foreach(sdio_clock_compile ${sdio_clock_target} ${sdio_clock_target}_sdio_ll)
    target_include_directories(${sdio_clock_compile} BEFORE PRIVATE
        "${sdio_clock_bundle}")
    target_compile_options(${sdio_clock_compile} BEFORE PRIVATE
        "-include${sdio_clock_bundle}/nexus_config.h")
endforeach()
