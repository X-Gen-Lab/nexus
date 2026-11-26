# Explicit io behavior targets and private model requirements.
# Author: Nexus Team
include("${nexus_google_root}/io/native_cold.cmake")

include("${nexus_google_root}/io/uart_formats.cmake")

include("${nexus_google_root}/io/fdcan.cmake")

include("${nexus_google_root}/io/flash_program.cmake")

include("${nexus_google_root}/io/external_bus_api.cmake")

nexus_google_test(nexus_owner_mock_test SOURCES "${nexus_google_root}/io/owner_mock_test.cpp"
    LIBRARIES Nexus::BusOwner
)

nexus_google_test(nexus_uart_owner_mock_test SOURCES "${nexus_google_root}/io/uart_owner_test.cpp"
    LIBRARIES Nexus::UARTOwner
)

nexus_google_test(nexus_spi_async_owner_mock_test SOURCES "${nexus_google_root}/io/spi_async_owner_test.cpp"
    LIBRARIES Nexus::SPIAsyncOwner Nexus::SpiOwner
)

nexus_google_test(nexus_hardware_contract_test
    SOURCES "${nexus_google_root}/io/dma_test.cpp" "${nexus_google_root}/io/stream_test.cpp" "${nexus_google_root}/os/wake_test.cpp"
    LIBRARIES Nexus::IO Nexus::Arch
)

nexus_google_test(nexus_i2c_bus_clear_test
    SOURCES "${nexus_google_root}/io/i2c_bus_clear_test.cpp"
    LIBRARIES Nexus::IO
)

nexus_google_test(nexus_uart_rs485_test SOURCES "${nexus_google_root}/io/uart_rs485_test.cpp"
    LIBRARIES Nexus::NativeModel Nexus::Arch
)

nexus_google_test(nexus_io_observer_test SOURCES "${nexus_google_root}/io/io_observer_test.cpp"
    LIBRARIES Nexus::OwnerObserver Nexus::IO
)

nexus_google_test(nexus_io_observation_contract_test
    SOURCES "${nexus_google_root}/io/io_observation_contract_test.cpp"
    LIBRARIES Nexus::IO
)

nexus_google_test(nexus_io_diagnostic_test
    SOURCES "${nexus_google_root}/io/io_diagnostic_test.cpp"
    LIBRARIES Nexus::IO
)

nexus_google_test(nexus_io_owner_diagnostic_pipeline_test
    SOURCES "${nexus_google_root}/io/io_owner_diagnostic_pipeline_test.cpp"
    LIBRARIES Nexus::OwnerObserver Nexus::IO Nexus::UARTOwner
        Nexus::SPIAsyncOwner Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers"
)

nexus_google_test(nexus_static_lifecycle_test SOURCES "${nexus_google_root}/io/static_lifecycle_test.cpp"
    LIBRARIES Nexus::Core
)

nexus_google_test(nexus_spi_dma_transaction_test
    SOURCES "${nexus_google_root}/io/spi_dma_transaction_test.cpp"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/io/private"
)

nexus_google_test(nexus_io_state_machine_test SOURCES "${nexus_google_root}/io/io_state_machine_test.cpp"
    LIBRARIES Nexus::OwnerObserver Nexus::IO Nexus::UARTOwner
        Nexus::SPIAsyncOwner Nexus::NativeModel
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers"
)

nexus_google_test(nexus_stream_storage_test SOURCES "${nexus_google_root}/io/stream_storage_test.cpp"
    LIBRARIES Nexus::IO Nexus::Arch
)

nexus_google_test(nexus_stream_profile_test SOURCES "${nexus_google_root}/io/stream_profile_test.cpp"
    LIBRARIES Nexus::IO Nexus::Arch
)

nexus_google_test(nexus_spi_dma_owner_test SOURCES "${nexus_google_root}/io/spi_dma_owner_test.cpp"
    LIBRARIES Nexus::SPIDMATransactionOwner Nexus::NativeModel Nexus::Arch
)

nexus_google_test(nexus_native_spi_dma_owner_test
    SOURCES "${nexus_google_root}/io/native_spi_dma_owner_test.cpp"
        "${NEXUS_SOURCE_DIR}/tests/support/native/drivers/spi_dma_transaction.c"
    LIBRARIES Nexus::SPIDMATransactionOwner Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers"
        "${NEXUS_SOURCE_DIR}/io/private"
)

target_compile_options(
    nexus_native_spi_dma_owner_test
    PRIVATE "-include${nexus_google_root}/io/native_spi_dma_owner_model.h"
)

nexus_google_test(nexus_spi_dma_owner_wait_test
    SOURCES "${nexus_google_root}/io/spi_dma_owner_wait_test.cpp"
    LIBRARIES Nexus::SPIDMATransactionOwner Nexus::NativeModel Nexus::OSNative
        Nexus::OwnerObserver Threads::Threads Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers"
        "${NEXUS_SOURCE_DIR}/io/private"
)

nexus_google_test(nexus_flash_provenance_test SOURCES "${nexus_google_root}/io/flash_provenance_test.cpp"
    LIBRARIES Nexus::FlashStorage Nexus::NativeModel
)

nexus_google_test(nexus_native_instances_test SOURCES "${nexus_google_root}/io/native_instances_test.cpp"
    LIBRARIES Nexus::NativeModel
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers"
)

nexus_google_test(nexus_native_async_test SOURCES "${nexus_google_root}/io/native_async_test.cpp"
    LIBRARIES Nexus::NativeModel
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers"
)

# A second assembly is a separate image, never linked to the default platform.
set(native_factory_input
    "${NEXUS_SOURCE_DIR}/tests/integration/assemblies/native_assembly/multi.toml"
)

set(native_factory_generated "${CMAKE_CURRENT_BINARY_DIR}/native-factory")

set_property(
    DIRECTORY
    APPEND
    PROPERTY
        CMAKE_CONFIGURE_DEPENDS
            "${native_factory_input}"
            "${NEXUS_SOURCE_DIR}/tests/integration/assemblies/native_assembly/multi_board/board.json"
)

execute_process(
    COMMAND
        "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py" --test-support
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${native_factory_input}"
        --output "${native_factory_generated}"
    RESULT_VARIABLE native_factory_result
    OUTPUT_VARIABLE native_factory_output
    ERROR_VARIABLE native_factory_error
)

if(NOT native_factory_result EQUAL 0)
    message(
        FATAL_ERROR
        "Native factory fixture rejected: "
        "${native_factory_output}${native_factory_error}"
    )
endif()

nexus_google_configuration_inputs("${native_factory_generated}")

nexus_google_test(nexus_native_factory_test SOURCES "${nexus_google_root}/io/native_factory_test.cpp"
    "${native_factory_generated}/bindings.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${native_factory_generated}" "${NEXUS_SOURCE_DIR}/tests/support/native/drivers"
)

include("${nexus_google_root}/io/spi_transaction.cmake")

include("${nexus_google_root}/io/spi_dma_owner_factory.cmake")

include("${nexus_google_root}/io/stack_observation.cmake")

include("${nexus_google_root}/io/ethernet.cmake")

include("${nexus_google_root}/io/usb_device.cmake")

include("${nexus_google_root}/io/failstop.cmake")

include("${nexus_google_root}/io/rtc_lptim.cmake")

include("${nexus_google_root}/io/frame_stream.cmake")

include("${nexus_google_root}/io/i3c_controller.cmake")
