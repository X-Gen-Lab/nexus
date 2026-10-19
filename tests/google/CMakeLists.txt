# Host-only framework: firmware and source-SDK consumers never enter this tree.
if(CMAKE_CROSSCOMPILING OR NOT NEXUS_SOC_FAMILY STREQUAL "native")
    message(FATAL_ERROR "GoogleTest/GoogleMock require the Native test profile")
endif()
include("${NEXUS_SOURCE_DIR}/cmake/testing/GoogleTest.cmake")
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
    target_compile_features(${name} PRIVATE cxx_std_17)
    set_target_properties(${name} PROPERTIES CXX_EXTENSIONS OFF)
    target_link_libraries(${name} PRIVATE nexus_build_options nexus_owned_warnings
        GTest::gmock_main ${ARG_LIBRARIES})
    if(nexus_google_compressed_debug_enabled)
        target_link_options(${name} PRIVATE
            "-Wl,--compress-debug-sections=zlib-gabi")
    endif()
    target_include_directories(${name} PRIVATE ${ARG_INCLUDES})
    if(ARG_STM32F4_LL)
        include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
        nexus_stm32f4_official(${name})
    endif()
    add_dependencies(nexus_google_contracts ${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 30 LABELS "GoogleTest")
endfunction()

include("${CMAKE_CURRENT_LIST_DIR}/g474_pin_state.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/uart_v1_channel_dma.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/uart_formats.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cycle_probe.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/adc_control.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/g474_timer_modes.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/g474_control_factory.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/g474_timer_factory.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/g474_pwm_adc.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/u575_uart_gpdma.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/u575_uart_gpdma_factory.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/g474_pwm_adc_factory.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/fdcan.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/g474_fdcan_factory.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/h743_flash_ram_sources.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/h743_high.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/legacy_devices.cmake")

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
nexus_google_test(nexus_silicon_admission_test
    SOURCES silicon_admission_test.cpp
    LIBRARIES Nexus::Core
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/common")
nexus_google_test(nexus_spi_dma_transaction_test
    SOURCES spi_dma_transaction_test.cpp
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/io/private")
nexus_google_test(nexus_io_state_machine_test SOURCES io_state_machine_test.cpp
    LIBRARIES Nexus::OwnerObserver Nexus::IO Nexus::UARTOwner
        Nexus::SPIAsyncOwner Nexus::NativeModel
    INCLUDES "${NEXUS_SOURCE_DIR}/io/native")
nexus_google_test(nexus_stream_storage_test SOURCES stream_storage_test.cpp
    LIBRARIES Nexus::IO Nexus::Arch)
nexus_google_test(nexus_stream_profile_test SOURCES stream_profile_test.cpp
    LIBRARIES Nexus::IO Nexus::Arch)
nexus_google_test(nexus_spi_dma_owner_test SOURCES spi_dma_owner_test.cpp
    LIBRARIES Nexus::SPIDMATransactionOwner Nexus::NativeModel Nexus::Arch)
nexus_google_test(nexus_native_spi_dma_owner_test
    SOURCES native_spi_dma_owner_test.cpp
        "${NEXUS_SOURCE_DIR}/io/native/spi_dma_transaction.c"
    LIBRARIES Nexus::SPIDMATransactionOwner Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/io/native"
        "${NEXUS_SOURCE_DIR}/io/private")
target_compile_options(nexus_native_spi_dma_owner_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/native_spi_dma_owner_model.h")
nexus_google_test(nexus_spi_dma_owner_wait_test
    SOURCES spi_dma_owner_wait_test.cpp
    LIBRARIES Nexus::SPIDMATransactionOwner Nexus::NativeModel Nexus::OSNative
        Nexus::OwnerObserver Threads::Threads Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/io/native"
        "${NEXUS_SOURCE_DIR}/io/private")
nexus_google_test(nexus_flash_provenance_test SOURCES flash_provenance_test.cpp
    LIBRARIES Nexus::FlashStorage Nexus::NativeModel)
nexus_google_test(nexus_native_instances_test SOURCES native_instances_test.cpp
    LIBRARIES Nexus::NativeModel
    INCLUDES "${NEXUS_SOURCE_DIR}/io/native")
nexus_google_test(nexus_native_async_test SOURCES native_async_test.cpp
    LIBRARIES Nexus::NativeModel
    INCLUDES "${NEXUS_SOURCE_DIR}/io/native")
nexus_google_test(nexus_irq_wake_provider_test SOURCES irq_wake_test.cpp
    LIBRARIES Nexus::NativeModel
    INCLUDES "${NEXUS_SOURCE_DIR}/io/native")

nexus_google_test(nexus_freertos_services_kernel_test
    SOURCES freertos_services_kernel_test.cpp freertos_stack_kernel_test.cpp
        freertos_owner_endurance_test.cpp
        "${NEXUS_SOURCE_DIR}/os/freertos/stack_snapshot.c"
    LIBRARIES contract_os_freertos_posix Nexus::UARTOwner
    INCLUDES "${NEXUS_SOURCE_DIR}/io/native")

# A second assembly is a separate image, never linked to the default platform.
set(native_factory_input
    "${NEXUS_SOURCE_DIR}/tests/contracts/native_assembly/multi.toml")
set(native_factory_generated "${CMAKE_CURRENT_BINARY_DIR}/native-factory")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${native_factory_input}"
    "${NEXUS_SOURCE_DIR}/tests/contracts/native_assembly/multi_board/board.json")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${native_factory_input}"
    --output "${native_factory_generated}"
    RESULT_VARIABLE native_factory_result
    OUTPUT_VARIABLE native_factory_output ERROR_VARIABLE native_factory_error)
if(NOT native_factory_result EQUAL 0)
    message(FATAL_ERROR "Native factory fixture rejected: "
        "${native_factory_output}${native_factory_error}")
endif()
file(READ "${native_factory_generated}/input_paths.json" native_factory_inputs)
string(JSON native_factory_count LENGTH "${native_factory_inputs}")
if(native_factory_count GREATER 0)
    math(EXPR native_factory_last "${native_factory_count} - 1")
    foreach(index RANGE 0 ${native_factory_last})
        string(JSON name MEMBER "${native_factory_inputs}" ${index})
        string(JSON input GET "${native_factory_inputs}" "${name}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input}")
    endforeach()
endif()
nexus_google_test(nexus_native_factory_test SOURCES native_factory_test.cpp
    "${native_factory_generated}/bindings.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${native_factory_generated}" "${NEXUS_SOURCE_DIR}/io/native")

nexus_google_test(nexus_stm32_uart_dma_test STM32F4_LL SOURCES stm32_uart_dma_test.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/uart.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32f407/private")
target_include_directories(nexus_stm32_uart_dma_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_stm32_uart_dma_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx)

nexus_google_test(nexus_irq_wake_stm32_test STM32F4_LL SOURCES irq_wake_stm32_test.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/uart.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32f407/private")
target_include_directories(nexus_irq_wake_stm32_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_irq_wake_stm32_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx)

target_sources(nexus_stm32_uart_dma_test PRIVATE
    "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/uart_dma.c")

target_compile_options(nexus_stm32_uart_dma_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_uart_dma_model.h")

# The host model compiles production register algorithms for two GD controllers.
set(gd_multi_root "${NEXUS_SOURCE_DIR}/soc/gd32f470")
set(gd_multi_vendor "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
include("${CMAKE_CURRENT_SOURCE_DIR}/spi_transaction.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/spi_dma_owner_factory.cmake")
nexus_google_test(nexus_irq_wake_gd32_test SOURCES irq_wake_gd32_test.cpp
    "${gd_multi_root}/drivers/uart.c"
    "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model/model.c"
    LIBRARIES Nexus::IO
    INCLUDES "${gd_multi_root}" "${gd_multi_root}/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_irq_wake_gd32_test SYSTEM PRIVATE
    "${gd_multi_vendor}/CMSIS/GD/GD32F4xx/Include"
    "${gd_multi_vendor}/GD32F4xx_standard_peripheral/Include")
target_compile_definitions(nexus_irq_wake_gd32_test PRIVATE GD32F470
    HXTAL_VALUE=25000000)
target_compile_options(nexus_irq_wake_gd32_test PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
nexus_google_test(nexus_gd32_multi_test SOURCES gd32_multi_test.cpp
    "${gd_multi_root}/drivers/uart.c" "${gd_multi_root}/drivers/spi.c"
    "${gd_multi_root}/drivers/gpio.c"
    "${gd_multi_root}/drivers/i2c.c"
    "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model/model.c"
    LIBRARIES Nexus::IO
    INCLUDES "${gd_multi_root}" "${gd_multi_root}/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
target_include_directories(nexus_gd32_multi_test SYSTEM PRIVATE
    "${gd_multi_vendor}/CMSIS/GD/GD32F4xx/Include"
    "${gd_multi_vendor}/GD32F4xx_standard_peripheral/Include")
target_compile_definitions(nexus_gd32_multi_test PRIVATE GD32F470
    HXTAL_VALUE=25000000)
target_compile_options(nexus_gd32_multi_test PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")

nexus_google_test(nexus_stm32_spi_dma_test STM32F4_LL
    SOURCES stm32_spi_dma_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/spi.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/gpio.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32f407/private")
target_include_directories(nexus_stm32_spi_dma_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_stm32_spi_dma_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx)
target_compile_options(nexus_stm32_spi_dma_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_dma_model.h")

target_sources(nexus_stm32_spi_dma_test PRIVATE
    "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/spi_dma.c")
target_include_directories(nexus_stm32_spi_dma_test PRIVATE
    "${NEXUS_SOURCE_DIR}/io/private")

nexus_google_test(nexus_gd32_uart_dma_test SOURCES gd32_uart_dma_test.cpp
    "${gd_multi_root}/drivers/uart.c" "${gd_multi_root}/drivers/uart_dma.c"
    "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model/model.c"
    LIBRARIES Nexus::IO
    INCLUDES "${gd_multi_root}" "${gd_multi_root}/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
target_include_directories(nexus_gd32_uart_dma_test SYSTEM PRIVATE
    "${gd_multi_vendor}/CMSIS/GD/GD32F4xx/Include"
    "${gd_multi_vendor}/GD32F4xx_standard_peripheral/Include")
target_compile_definitions(nexus_gd32_uart_dma_test PRIVATE GD32F470
    HXTAL_VALUE=25000000)
target_compile_options(nexus_gd32_uart_dma_test PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>"
    "$<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/gd32_uart_dma_model.h>")

nexus_google_test(nexus_stm32_uart_receive_test STM32F4_LL
    SOURCES stm32_uart_receive_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/uart_dma.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/uart_receive.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/uart_duplex.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32f407/private")
target_include_directories(nexus_stm32_uart_receive_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_stm32_uart_receive_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx)
target_compile_options(nexus_stm32_uart_receive_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_uart_receive_model.h")

nexus_google_test(nexus_gd32_uart_receive_test
    SOURCES gd32_uart_receive_test.cpp
        "${gd_multi_root}/drivers/uart.c"
        "${gd_multi_root}/drivers/uart_dma.c"
        "${gd_multi_root}/drivers/uart_receive.c"
        "${gd_multi_root}/drivers/uart_duplex.c"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model/model.c"
    LIBRARIES Nexus::IO
    INCLUDES "${gd_multi_root}" "${gd_multi_root}/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
target_include_directories(nexus_gd32_uart_receive_test SYSTEM PRIVATE
    "${gd_multi_vendor}/CMSIS/GD/GD32F4xx/Include"
    "${gd_multi_vendor}/GD32F4xx_standard_peripheral/Include")
target_compile_definitions(nexus_gd32_uart_receive_test PRIVATE
    GD32F470 HXTAL_VALUE=25000000)
target_compile_options(nexus_gd32_uart_receive_test PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>"
    "$<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/gd32_uart_receive_model.h>")

nexus_google_test(nexus_stm32_uart_stream_test STM32F4_LL
    SOURCES stm32_uart_stream_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/uart.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32f407/private")
target_include_directories(nexus_stm32_uart_stream_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_stm32_uart_stream_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx)

target_sources(nexus_stm32_uart_stream_test PRIVATE
    "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/uart_stream.c")

nexus_google_test(nexus_gd32_spi_dma_test SOURCES gd32_spi_dma_test.cpp
    "${gd_multi_root}/drivers/spi.c" "${gd_multi_root}/drivers/spi_dma.c"
    "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model/model.c"
    LIBRARIES Nexus::IO
    INCLUDES "${gd_multi_root}" "${gd_multi_root}/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model"
        "${NEXUS_SOURCE_DIR}/io/private")
target_include_directories(nexus_gd32_spi_dma_test SYSTEM PRIVATE
    "${gd_multi_vendor}/CMSIS/GD/GD32F4xx/Include"
    "${gd_multi_vendor}/GD32F4xx_standard_peripheral/Include")
target_compile_definitions(nexus_gd32_spi_dma_test PRIVATE GD32F470
    HXTAL_VALUE=25000000)
target_compile_options(nexus_gd32_spi_dma_test PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>"
    "$<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/gd32_spi_dma_model.h>")

nexus_google_test(nexus_stm32_adc_stream_test STM32F4_LL
    SOURCES stm32_adc_stream_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/adc.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32f407/private")
target_include_directories(nexus_stm32_adc_stream_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_stm32_adc_stream_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx)
target_compile_options(nexus_stm32_adc_stream_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_adc_stream_model.h")

nexus_google_test(nexus_gd32_flash_fault_test SOURCES gd32_flash_fault_test.cpp
    "${gd_multi_root}/drivers/flash.c"
    "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model/model.c"
    LIBRARIES Nexus::IO
    INCLUDES "${gd_multi_root}" "${gd_multi_root}/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
target_include_directories(nexus_gd32_flash_fault_test SYSTEM PRIVATE
    "${gd_multi_vendor}/CMSIS/GD/GD32F4xx/Include"
    "${gd_multi_vendor}/GD32F4xx_standard_peripheral/Include")
target_compile_definitions(nexus_gd32_flash_fault_test PRIVATE GD32F470
    HXTAL_VALUE=25000000)
target_compile_options(nexus_gd32_flash_fault_test PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")

target_sources(nexus_stm32_adc_stream_test PRIVATE
    "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/adc_stream.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/adc_continuous.c")

nexus_google_test(nexus_gd32_adc_stream_test SOURCES gd32_adc_stream_test.cpp
    "${gd_multi_root}/drivers/adc.c" "${gd_multi_root}/drivers/adc_stream.c"
    "${gd_multi_root}/drivers/adc_continuous.c"
    "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model/model.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${gd_multi_root}" "${gd_multi_root}/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
target_include_directories(nexus_gd32_adc_stream_test SYSTEM PRIVATE
    "${gd_multi_vendor}/CMSIS/GD/GD32F4xx/Include"
    "${gd_multi_vendor}/GD32F4xx_standard_peripheral/Include")
target_compile_definitions(nexus_gd32_adc_stream_test PRIVATE GD32F470
    HXTAL_VALUE=25000000)
target_compile_options(nexus_gd32_adc_stream_test PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>"
    "$<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/gd32_adc_stream_model.h>")

nexus_google_test(nexus_gd32_adc_initialize_test
    SOURCES gd32_adc_initialize_test.cpp "${gd_multi_root}/drivers/adc.c"
    LIBRARIES Nexus::IO
    INCLUDES "${gd_multi_root}" "${gd_multi_root}/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
target_include_directories(nexus_gd32_adc_initialize_test SYSTEM PRIVATE
    "${gd_multi_vendor}/CMSIS/GD/GD32F4xx/Include"
    "${gd_multi_vendor}/GD32F4xx_standard_peripheral/Include")
target_compile_definitions(nexus_gd32_adc_initialize_test PRIVATE GD32F470
    HXTAL_VALUE=25000000)
target_compile_options(nexus_gd32_adc_initialize_test PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")

nexus_google_test(nexus_gd32_uart_stream_test SOURCES gd32_uart_stream_test.cpp
    "${gd_multi_root}/drivers/uart.c" "${gd_multi_root}/drivers/uart_stream.c"
    "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model/model.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${gd_multi_root}" "${gd_multi_root}/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
target_include_directories(nexus_gd32_uart_stream_test SYSTEM PRIVATE
    "${gd_multi_vendor}/CMSIS/GD/GD32F4xx/Include"
    "${gd_multi_vendor}/GD32F4xx_standard_peripheral/Include")
target_compile_definitions(nexus_gd32_uart_stream_test PRIVATE GD32F470
    HXTAL_VALUE=25000000)
target_compile_options(nexus_gd32_uart_stream_test PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")

nexus_google_test(nexus_stm32_i2c_register_test STM32F4_LL
    SOURCES i2c_register_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/i2c.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32f407/private"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_stm32_i2c_register_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_stm32_i2c_register_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx NEXUS_I2C_STM32_TEST)
target_compile_options(nexus_stm32_i2c_register_test PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/i2c_register_model.h>")

nexus_google_test(nexus_gd32_i2c_register_test
    SOURCES i2c_register_test.cpp "${gd_multi_root}/drivers/i2c.c"
    LIBRARIES Nexus::IO
    INCLUDES "${gd_multi_root}" "${gd_multi_root}/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
target_include_directories(nexus_gd32_i2c_register_test SYSTEM PRIVATE
    "${gd_multi_vendor}/CMSIS/GD/GD32F4xx/Include"
    "${gd_multi_vendor}/GD32F4xx_standard_peripheral/Include")
target_compile_definitions(nexus_gd32_i2c_register_test PRIVATE GD32F470
    HXTAL_VALUE=25000000)
target_compile_options(nexus_gd32_i2c_register_test PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>"
    "$<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/i2c_register_model.h>")

foreach(i2c_family generic stm32 gd32)
    set(i2c_target "nexus_${i2c_family}_i2c_async_test")
    nexus_google_test(${i2c_target} SOURCES i2c_async_register_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/common/i2c_v1_async.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/common")
    target_compile_options(${i2c_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/i2c_async_register_model.h")
    if(i2c_family STREQUAL "stm32")
        target_sources(${i2c_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/i2c_async.c")
        target_include_directories(${i2c_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/stm32f407/private")
        target_include_directories(${i2c_target} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
            "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
        target_compile_definitions(${i2c_target} PRIVATE
            NX_I2C_TEST_STM32 NEXUS_STM32_MODEL STM32F407xx)
    elseif(i2c_family STREQUAL "gd32")
        target_sources(${i2c_target} PRIVATE
            "${gd_multi_root}/drivers/i2c_async.c")
        target_include_directories(${i2c_target} PRIVATE
            "${gd_multi_root}/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
        target_include_directories(${i2c_target} SYSTEM PRIVATE
            "${gd_multi_vendor}/CMSIS/GD/GD32F4xx/Include"
            "${gd_multi_vendor}/GD32F4xx_standard_peripheral/Include")
        target_compile_definitions(${i2c_target} PRIVATE
            NX_I2C_TEST_GD32 GD32F470 HXTAL_VALUE=25000000)
        target_compile_options(${i2c_target} PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
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
include(os_optional.cmake)
include(os_secure.cmake)
include(freertos_mpu.cmake)
include(gd32_arch.cmake)
include(gd32_memory.cmake)
include(stm32_memory.cmake)

# GD32 tests replace the CPU boundary coherently; no partial Native Arch model.
foreach(gd_target nexus_gd32_multi_test nexus_gd32_uart_dma_test
        nexus_gd32_uart_receive_test
        nexus_gd32_spi_dma_test nexus_gd32_flash_fault_test
        nexus_gd32_adc_stream_test nexus_gd32_adc_initialize_test
        nexus_gd32_uart_stream_test nexus_gd32_i2c_register_test)
    target_include_directories(${gd_target} PRIVATE
        "${NEXUS_SOURCE_DIR}/arch/include")
endforeach()

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

include(i2c_pair_factory.cmake)

include(physical_platform_factory.cmake)
include(timebase_lowpower.cmake)

include(usb_factory.cmake)

include(clock_domain.cmake)

include(g474.cmake)
include(g474_exti.cmake)

include(g474_factory.cmake)

include(threadx.cmake)

include(i2c_multi_controller.cmake)

include(user_io_broker.cmake)

include(failstop.cmake)

include(ethernet_factory.cmake)

include(dma_multi_controller.cmake)

include(soc_system.cmake)
include(gd32_exti.cmake)
include(stm32_exti.cmake)
include(stm32_official.cmake)
include(h743.cmake)
include(h743_spi_wire.cmake)
include(h743_dma.cmake)
include(workload_uart3.cmake)
include(u575.cmake)
include(u575_factory.cmake)
include(u575_security.cmake)
include(gd32f303.cmake)
include(g474_mainstream.cmake)
include(u575_mainstream.cmake)
include(stm32f103.cmake)
include(legacy_watchdog.cmake)
include(h743_flash_execution.cmake)
include(g474_adc_group.cmake)
include(ethernet_raw_errata.cmake)
include(h743_uart_fifo.cmake)
include(u575_uart_fifo.cmake)
include(h743_uart_rtscts.cmake)
include(u575_uart_rtscts.cmake)
include(ethernet_frames.cmake)
include(h743_flash_image_admission.cmake)
include(g474_adc_group_factory.cmake)
include(h743_uart_fifo_factory.cmake)
include(u575_uart_fifo_factory.cmake)
include(h743_uart_rtscts_factory.cmake)
include(u575_uart_rtscts_factory.cmake)
include(ethernet_frames_factory.cmake)
include(i2c_v2_long_factory.cmake)
include(g474_pwm_shared.cmake)
include(g474_pwm_shared_factory.cmake)
include(h743_flash_ram.cmake)
include(h743_flash_ram_closure.cmake)
include(h743_flash_mpu.cmake)

include("${CMAKE_CURRENT_SOURCE_DIR}/rtc_lptim.cmake")

include(h743_spi_dma.cmake)

include(h743_spi_dma_factory.cmake)
include("${CMAKE_CURRENT_SOURCE_DIR}/h743_runtime_clock.cmake")
