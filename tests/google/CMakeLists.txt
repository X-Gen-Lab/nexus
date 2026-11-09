# Host-only framework: firmware and source-SDK consumers never enter this tree.
if(CMAKE_CROSSCOMPILING OR NOT NEXUS_SOC_FAMILY STREQUAL "native")
    message(FATAL_ERROR "GoogleTest/GoogleMock require the Native test profile")
endif()
include("${NEXUS_SOURCE_DIR}/cmake/testing/GoogleTest.cmake")
include("${NEXUS_SOURCE_DIR}/tests/cmake/CMSISHostModel.cmake")
nexus_host_google_test()

# Preserve complete host DWARF without repeating uncompressed framework debug
# sections in every contract executable. Firmware never enters this directory.
set(nexus_google_compressed_debug_enabled FALSE)
if(UNIX AND NOT APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    include(CheckLinkerFlag)
    check_linker_flag(CXX "-Wl,--compress-debug-sections=zlib-gabi"
        nexus_google_compressed_debug)
    if(nexus_google_compressed_debug)
        set(nexus_google_compressed_debug_enabled TRUE)
    endif()
endif()

add_custom_target(nexus_google_contracts)
function(nexus_google_test name)
    cmake_parse_arguments(ARG "STM32F4_LL" "" "SOURCES;LIBRARIES;INCLUDES" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    # Only generated bindings use the compile-visible Core coordinator. Keep
    # that private include off unrelated test sources and compiler contexts.
    foreach(source IN LISTS ARG_SOURCES)
        if(source MATCHES "/bindings\\.c$")
            set_property(SOURCE "${source}" APPEND PROPERTY INCLUDE_DIRECTORIES
                "${NEXUS_SOURCE_DIR}/core/private")
        endif()
    endforeach()
    target_include_directories(${name} PRIVATE
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ip/i2c_v1_async"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ip/classic_can"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ip/fs_device_pio"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ip/legacy_mac"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ip/advanced_timer")
    target_compile_features(${name} PRIVATE cxx_std_17)
    set_target_properties(${name} PROPERTIES CXX_EXTENSIONS OFF)
    target_link_libraries(${name} PRIVATE nexus_build_options nexus_owned_warnings
        GTest::gmock_main ${ARG_LIBRARIES})
    nexus_use_cmsis_host_model(${name})
    if(nexus_google_compressed_debug_enabled)
        target_link_options(${name} PRIVATE
            "-Wl,--compress-debug-sections=zlib-gabi")
    endif()
    target_include_directories(${name} PRIVATE ${ARG_INCLUDES})
    if(ARG_STM32F4_LL)
        include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
        nexus_stm32f4_official(${name})
    endif()
    set_property(TARGET ${name} PROPERTY NEXUS_GOOGLE_CONTRACT TRUE)
    add_dependencies(nexus_google_contracts ${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 30 LABELS "GoogleTest")
endfunction()

include("${CMAKE_CURRENT_SOURCE_DIR}/dac.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/crc.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/native_cold.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/stm32_i2c_cold.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/stm32_platform_cold.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/stm32_static_lifecycle.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/stm32_i2c_poll_factory.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/stm32_control_factory.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/stm32_uart_cold_factory.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/stm32_basic_cold.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/stm32_adc_cold_factory.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/uart_formats.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cycle_probe.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/adc_control.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/fdcan.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/flash_program.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/qiming_touch.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/ili9341.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/external_bus_api.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/at24c02.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/w25.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/software_i2c.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/f407_fsmc.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/f407_rtc.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/stm32_sdio_block.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/f407_sram_boot.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/qiming_boot_platform.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/infrared.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/infrared_exti.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/onewire.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/ds18b20.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/qiming_rs485_factory.cmake")

nexus_google_test(nexus_owner_mock_test SOURCES owner_mock_test.cpp
    LIBRARIES Nexus::BusOwner)
nexus_google_test(nexus_realtime_metadata_test SOURCES realtime_metadata_test.cpp
    LIBRARIES Nexus::BusOwner Nexus::IO Nexus::Arch)
nexus_google_test(nexus_polymorphic_io_test SOURCES polymorphic_mock_test.cpp
    LIBRARIES Nexus::IO)
nexus_google_test(nexus_uart_owner_mock_test SOURCES uart_owner_test.cpp
    LIBRARIES Nexus::UARTOwner)
nexus_google_test(nexus_spi_async_owner_mock_test SOURCES spi_async_owner_test.cpp
    LIBRARIES Nexus::SPIAsyncOwner Nexus::SpiOwner)
nexus_google_test(nexus_hardware_contract_test
    SOURCES dma_test.cpp stream_test.cpp wake_test.cpp
    LIBRARIES Nexus::IO Nexus::Arch)
nexus_google_test(nexus_i2c_bus_clear_test
    SOURCES i2c_bus_clear_test.cpp
    LIBRARIES Nexus::IO)
nexus_google_test(nexus_uart_rs485_test SOURCES uart_rs485_test.cpp
    LIBRARIES Nexus::NativeModel Nexus::Arch)
nexus_google_test(nexus_io_observer_test SOURCES io_observer_test.cpp
    LIBRARIES Nexus::OwnerObserver Nexus::IO)
nexus_google_test(nexus_io_observation_contract_test
    SOURCES io_observation_contract_test.cpp
    LIBRARIES Nexus::IO)
nexus_google_test(nexus_io_diagnostic_test
    SOURCES io_diagnostic_test.cpp
    LIBRARIES Nexus::IO)
nexus_google_test(nexus_io_owner_diagnostic_pipeline_test
    SOURCES io_owner_diagnostic_pipeline_test.cpp
    LIBRARIES Nexus::OwnerObserver Nexus::IO Nexus::UARTOwner
        Nexus::SPIAsyncOwner Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers")
nexus_google_test(nexus_silicon_admission_test
    SOURCES silicon_admission_test.cpp
    LIBRARIES Nexus::Core
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
nexus_google_test(nexus_core_lifecycle_test SOURCES core_lifecycle_test.cpp
    LIBRARIES Nexus::Core)
nexus_google_test(nexus_static_lifecycle_test SOURCES static_lifecycle_test.cpp
    LIBRARIES Nexus::Core)
nexus_google_test(nexus_platform_lifecycle_test SOURCES platform_lifecycle_test.cpp
    LIBRARIES Nexus::Core)
nexus_google_test(nexus_stm32_platform_coordinator_test STM32F4_LL
    SOURCES stm32_platform_coordinator_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_coordinator.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/tests/support/f4_cpu"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_stm32_platform_coordinator_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
target_compile_definitions(nexus_stm32_platform_coordinator_test PRIVATE
    STM32F407xx)
nexus_google_test(nexus_spi_dma_transaction_test
    SOURCES spi_dma_transaction_test.cpp
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/io/private")
nexus_google_test(nexus_io_state_machine_test SOURCES io_state_machine_test.cpp
    LIBRARIES Nexus::OwnerObserver Nexus::IO Nexus::UARTOwner
        Nexus::SPIAsyncOwner Nexus::NativeModel
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers")
nexus_google_test(nexus_stream_storage_test SOURCES stream_storage_test.cpp
    LIBRARIES Nexus::IO Nexus::Arch)
nexus_google_test(nexus_stream_profile_test SOURCES stream_profile_test.cpp
    LIBRARIES Nexus::IO Nexus::Arch)
nexus_google_test(nexus_spi_dma_owner_test SOURCES spi_dma_owner_test.cpp
    LIBRARIES Nexus::SPIDMATransactionOwner Nexus::NativeModel Nexus::Arch)
nexus_google_test(nexus_native_spi_dma_owner_test
    SOURCES native_spi_dma_owner_test.cpp
        "${NEXUS_SOURCE_DIR}/tests/support/native/drivers/spi_dma_transaction.c"
    LIBRARIES Nexus::SPIDMATransactionOwner Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers"
        "${NEXUS_SOURCE_DIR}/io/private")
target_compile_options(nexus_native_spi_dma_owner_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/native_spi_dma_owner_model.h")
nexus_google_test(nexus_spi_dma_owner_wait_test
    SOURCES spi_dma_owner_wait_test.cpp
    LIBRARIES Nexus::SPIDMATransactionOwner Nexus::NativeModel Nexus::OSNative
        Nexus::OwnerObserver Threads::Threads Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers"
        "${NEXUS_SOURCE_DIR}/io/private")
nexus_google_test(nexus_flash_provenance_test SOURCES flash_provenance_test.cpp
    LIBRARIES Nexus::FlashStorage Nexus::NativeModel)
nexus_google_test(nexus_tmp117_test SOURCES tmp117_test.cpp
    LIBRARIES Nexus::TMP117I2C Nexus::BusOwner Nexus::Arch)
nexus_google_test(nexus_settings_test SOURCES settings_test.cpp
    LIBRARIES Nexus::Settings)
nexus_google_test(nexus_native_instances_test SOURCES native_instances_test.cpp
    LIBRARIES Nexus::NativeModel
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers")
nexus_google_test(nexus_native_async_test SOURCES native_async_test.cpp
    LIBRARIES Nexus::NativeModel
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers")
nexus_google_test(nexus_irq_wake_provider_test SOURCES irq_wake_test.cpp
    LIBRARIES Nexus::NativeModel
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers")

nexus_google_test(nexus_freertos_services_kernel_test
    SOURCES freertos_services_kernel_test.cpp freertos_stack_kernel_test.cpp
        freertos_owner_endurance_test.cpp
        "${NEXUS_SOURCE_DIR}/os/freertos/stack_snapshot.c"
    LIBRARIES contract_os_freertos_posix Nexus::UARTOwner
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers")

# A second assembly is a separate image, never linked to the default platform.
set(native_factory_input
    "${NEXUS_SOURCE_DIR}/tests/contracts/native_assembly/multi.toml")
set(native_factory_generated "${CMAKE_CURRENT_BINARY_DIR}/native-factory")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${native_factory_input}"
    "${NEXUS_SOURCE_DIR}/tests/contracts/native_assembly/multi_board/board.json")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py" --test-support
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${native_factory_input}"
    --output "${native_factory_generated}"
    RESULT_VARIABLE native_factory_result
    OUTPUT_VARIABLE native_factory_output ERROR_VARIABLE native_factory_error)
if(NOT native_factory_result EQUAL 0)
    message(FATAL_ERROR "Native factory fixture rejected: "
        "${native_factory_output}${native_factory_error}")
endif()
nexus_google_configuration_inputs("${native_factory_generated}")
nexus_google_test(nexus_native_factory_test SOURCES native_factory_test.cpp
    "${native_factory_generated}/bindings.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${native_factory_generated}" "${NEXUS_SOURCE_DIR}/tests/support/native/drivers")

nexus_google_test(nexus_stm32_uart_dma_test STM32F4_LL SOURCES stm32_uart_dma_test.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
target_include_directories(nexus_stm32_uart_dma_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_stm32_uart_dma_test)
target_compile_definitions(nexus_stm32_uart_dma_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx)

nexus_google_test(nexus_irq_wake_stm32_test STM32F4_LL SOURCES irq_wake_stm32_test.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
target_include_directories(nexus_irq_wake_stm32_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_irq_wake_stm32_test)
target_compile_definitions(nexus_irq_wake_stm32_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx)

target_sources(nexus_stm32_uart_dma_test PRIVATE
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart_dma.c")

target_compile_options(nexus_stm32_uart_dma_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_uart_dma_model.h")
include("${CMAKE_CURRENT_SOURCE_DIR}/spi_transaction.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/spi_dma_owner_factory.cmake")

nexus_google_test(nexus_stm32_spi_dma_test STM32F4_LL
    SOURCES stm32_spi_dma_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/spi.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
target_include_directories(nexus_stm32_spi_dma_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_stm32_spi_dma_test)
target_compile_definitions(nexus_stm32_spi_dma_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx)
target_compile_options(nexus_stm32_spi_dma_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_dma_model.h")

target_sources(nexus_stm32_spi_dma_test PRIVATE
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/spi_dma.c")
target_include_directories(nexus_stm32_spi_dma_test PRIVATE
    "${NEXUS_SOURCE_DIR}/io/private")

nexus_google_test(nexus_stm32_uart_receive_test STM32F4_LL
    SOURCES stm32_uart_receive_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart_dma.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart_receive.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart_duplex.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
target_include_directories(nexus_stm32_uart_receive_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_stm32_uart_receive_test)
target_compile_definitions(nexus_stm32_uart_receive_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx)
target_compile_options(nexus_stm32_uart_receive_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_uart_receive_model.h")

nexus_google_test(nexus_stm32_uart_stream_test STM32F4_LL
    SOURCES stm32_uart_stream_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
target_include_directories(nexus_stm32_uart_stream_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_stm32_uart_stream_test)
target_compile_definitions(nexus_stm32_uart_stream_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx)

target_sources(nexus_stm32_uart_stream_test PRIVATE
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart_stream.c")

nexus_google_test(nexus_stm32_adc_stream_test STM32F4_LL
    SOURCES stm32_adc_stream_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/adc.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
target_include_directories(nexus_stm32_adc_stream_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_stm32_adc_stream_test)
target_compile_definitions(nexus_stm32_adc_stream_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx)
target_compile_options(nexus_stm32_adc_stream_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_adc_stream_model.h")

target_sources(nexus_stm32_adc_stream_test PRIVATE
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/adc_stream.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/adc_continuous.c")

nexus_google_test(nexus_stm32_i2c_register_test STM32F4_LL
    SOURCES i2c_register_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/i2c.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/i2c_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio.c"
        stm32_i2c_storage_model.c
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_stm32_i2c_register_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_stm32_i2c_register_test)
target_compile_definitions(nexus_stm32_i2c_register_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx NEXUS_I2C_STM32_TEST)
target_compile_options(nexus_stm32_i2c_register_test PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/i2c_register_model.h>")

foreach(i2c_family generic stm32)
    set(i2c_target "nexus_${i2c_family}_i2c_async_test")
    nexus_google_test(${i2c_target} SOURCES i2c_async_register_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ip/i2c_v1_async/i2c_v1_async.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ip/i2c_v1_async")
    target_compile_options(${i2c_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/i2c_async_register_model.h")
    if(i2c_family STREQUAL "stm32")
        target_sources(${i2c_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/i2c_async.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/i2c_cold.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/gpio.c"
            stm32_i2c_storage_model.c)
        include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
        nexus_stm32f4_official(${i2c_target})
        target_include_directories(${i2c_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
        target_include_directories(${i2c_target} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
            "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
        nexus_use_cmsis_core(${i2c_target})
        target_compile_definitions(${i2c_target} PRIVATE
            NX_I2C_TEST_STM32 NEXUS_STM32_MODEL STM32F407xx)
    endif()
endforeach()

include(arch_cortex.cmake)
include(arch_native.cmake)
include(atomic_tests.cmake)
include(arch_cache.cmake)
include(arch_cache_control.cmake)
include(arch_mpu.cmake)
include(arch_security.cmake)
include(arch_nonsecure.cmake)
include(arch_native_mechanism.cmake)
include(freertos_context.cmake)
include(freertos_prestart.cmake)
include(os_wait.cmake)
nexus_google_test(nexus_wait_select_test SOURCES wait_select_test.cpp
    LIBRARIES Nexus::OSWait)
include(os_optional.cmake)
include(os_secure.cmake)
include(freertos_mpu.cmake)
include(stm32_memory.cmake)

include(plan.cmake)

include(usb_clock.cmake)

include(stack_observation.cmake)

include(can_factory.cmake)

include(can_runtime.cmake)
include(ethernet.cmake)
include(usb_device.cmake)
include(timer_advanced.cmake)

include(timer_factory.cmake)

include(multi_controller_factory.cmake)
include(spi_sequence_factory.cmake)

include(i2c_pair_factory.cmake)

include(physical_platform_factory.cmake)
include(timebase_lowpower.cmake)

include(usb_factory.cmake)

include(clock_domain.cmake)

include(threadx.cmake)

include(i2c_multi_controller.cmake)

include(user_io_broker.cmake)
include(user_io_async.cmake)

include(failstop.cmake)

include(ethernet_factory.cmake)

include(dma_multi_controller.cmake)

include(soc_system.cmake)
include(stm32_exti.cmake)
include(stm32_official.cmake)
include(stm32_uart_hal.cmake)
include(legacy_watchdog.cmake)
include(ethernet_raw_errata.cmake)
include(ethernet_frames.cmake)
include(ethernet_frames_factory.cmake)

include("${CMAKE_CURRENT_SOURCE_DIR}/rtc_lptim.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/mapped_storage.cmake")
include(amp_ipc.cmake)
include(stm32f407_handoff.cmake)
include(f407_i2s2_slave_tx.cmake)
include(frame_stream.cmake)

include(i3c_controller.cmake)

include(official_leaf_probes.cmake)

if(TARGET Nexus::LwIP)
    nexus_google_test(nexus_network_lwip_test
        SOURCES network_lwip_test.cpp LIBRARIES Nexus::LwIPStorage)
endif()

# Actual raw TCP peers own separate lwIP global states on the maintained host.
if(TARGET Nexus::LwIP AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    nexus_google_test(nexus_network_tcp_pair_test
        SOURCES network_tcp_pair_test.cpp
        LIBRARIES Nexus::LwIPStorage)
    set_tests_properties(nexus_network_tcp_pair_test PROPERTIES TIMEOUT 30)
endif()
if(TARGET Nexus::LwIPTCPStorage AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    nexus_google_test(nexus_network_tcp_stream_test
        SOURCES network_tcp_stream_test.cpp
        LIBRARIES Nexus::LwIPTCPStorage)
    target_link_options(nexus_network_tcp_stream_test PRIVATE
        "LINKER:--wrap=tcp_output" "LINKER:--wrap=tcp_close")
    set_tests_properties(nexus_network_tcp_stream_test PROPERTIES TIMEOUT 30)
endif()

# A selected RX payload-loan profile is a separate static stack domain.
if(TARGET Nexus::LwIPLoanedRXStorage AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    nexus_google_test(nexus_network_lwip_loaned_test
        SOURCES network_lwip_loaned_test.cpp LIBRARIES Nexus::LwIPLoanedRXStorage)
endif()
if(TARGET Nexus::LwIPTCPLoanedRXStorage AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    nexus_google_test(nexus_network_tcp_loaned_stream_test
        SOURCES network_tcp_stream_test.cpp
        LIBRARIES Nexus::LwIPTCPLoanedRXStorage)
    target_link_options(nexus_network_tcp_loaned_stream_test PRIVATE
        "LINKER:--wrap=tcp_output" "LINKER:--wrap=tcp_close")
    set_tests_properties(nexus_network_tcp_loaned_stream_test PROPERTIES TIMEOUT 30)
endif()

# The explicit entropy instance uses the real reviewed LL over a register model.
nexus_google_test(nexus_entropy_test STM32F4_LL SOURCES entropy_test.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/entropy.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/entropy_instance.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/include")
target_include_directories(nexus_entropy_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_entropy_test)
target_compile_definitions(nexus_entropy_test PRIVATE STM32F407xx
    NEXUS_STM32_MODEL)
target_compile_options(nexus_entropy_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/entropy_model.h")

include(tls_client.cmake)

nexus_add_coremqtt()
nexus_google_test(nexus_mqtt_client_test
    SOURCES mqtt_client_test.cpp LIBRARIES Nexus::CoreMQTTStorage)

# Compile the opt-in broker client without running Docker in ordinary hooks.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(nexus_mqtt_tls_interop_client EXCLUDE_FROM_ALL
        "${NEXUS_SOURCE_DIR}/tests/contracts/mqtt_tls_interop/client.c")
    nexus_apply_target_policy(nexus_mqtt_tls_interop_client)
    target_link_libraries(nexus_mqtt_tls_interop_client PRIVATE
        Nexus::TLSStorage Nexus::CoreMQTTStorage
        nexus_build_options nexus_owned_warnings)
    add_dependencies(nexus_tls_client_test nexus_mqtt_tls_interop_client)
endif()

# Compile the real client stack chain as a nested Google dependency. Broker
# execution stays explicit; ordinary hooks never start Docker or use a PHY.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
   TARGET nexus_network_tcp_stream_test AND TARGET Nexus::TLSStorage AND
   TARGET Nexus::CoreMQTTStorage)
    add_executable(nexus_lwip_mqtt_interop_client EXCLUDE_FROM_ALL
        "${NEXUS_SOURCE_DIR}/tests/contracts/lwip_mqtt_interop/client.c"
        "${NEXUS_SOURCE_DIR}/tests/contracts/lwip_mqtt_interop/transport.cpp")
    nexus_apply_target_policy(nexus_lwip_mqtt_interop_client)
    target_include_directories(nexus_lwip_mqtt_interop_client PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(nexus_lwip_mqtt_interop_client PRIVATE
        Nexus::LwIPTCPStorage Nexus::TLSStorage Nexus::CoreMQTTStorage
        GTest::gtest nexus_build_options nexus_owned_warnings)
    add_dependencies(nexus_network_tcp_stream_test
        nexus_lwip_mqtt_interop_client)
endif()

# The reviewed full boot model uses the supplier's ordinary Linux test port.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include("${CMAKE_CURRENT_SOURCE_DIR}/mcuboot.cmake")
endif()

# One external BSRR/ODR model, shared by every maintained STM32 SPI context.
foreach(spi_latch_target IN ITEMS
        contract_stm32_constructor_model contract_stm32_io_model
        nexus_stm32_multi_controller_factory_test
        nexus_stm32_single_spi_factory_test nexus_stm32_spi_dma_owner_test
        nexus_stm32_spi_dma_sequence_test nexus_stm32_spi_dma_test
        nexus_stm32_spi_sequence_factory_test
        nexus_stm32_spi_transaction_test nexus_stm32_static_lifecycle_test
        nexus_stm32f4_spi_multi_controller_test)
    target_sources(${spi_latch_target} PRIVATE stm32_spi_cs_model.c)
    target_compile_options(${spi_latch_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_spi_cs_model.h")
endforeach()
