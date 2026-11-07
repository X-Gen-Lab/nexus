# CTest owns every Native runtime contract in each compiler/sanitizer mode.
# Configuration, SDK and FreeRTOS overlay tests run through the complete
# configure/contracts/freertos tool domains; they do not inherit this mode.

function(nexus_behavior_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBRARIES;INCLUDES;DEFINITIONS" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE nexus_build_options nexus_owned_warnings
        ${ARG_LIBRARIES})
    target_include_directories(${name} PRIVATE ${ARG_INCLUDES})
    target_compile_definitions(${name} PRIVATE ${ARG_DEFINITIONS})
    if(MSVC)
        target_compile_options(${name} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${name} PRIVATE -UNDEBUG)
    endif()
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 30)
endfunction()
find_package(Threads REQUIRED)
nexus_behavior_test(contract_core_contract
    SOURCES core_contract_test.c LIBRARIES Nexus::Core Nexus::Arch Threads::Threads)
nexus_behavior_test(contract_native_io
    SOURCES native_io_test.c LIBRARIES Nexus::NativeModel)

nexus_behavior_test(contract_os_wait SOURCES os_wait.c
    LIBRARIES nexus_os_baremetal nexus_os_native)
nexus_behavior_test(contract_bus_owner SOURCES components_owner.c
    LIBRARIES nexus_bus_owner nexus_os_native Nexus::NativeModel)
nexus_behavior_test(contract_bmp280 SOURCES components_bmp280.c
    LIBRARIES nexus_bmp280)
nexus_behavior_test(contract_storage SOURCES components_storage.c
    LIBRARIES nexus_file_flash)
nexus_behavior_test(contract_modbus SOURCES components_modbus.c
    LIBRARIES nexus_modbus)
nexus_behavior_test(contract_log SOURCES components_log.c
    LIBRARIES nexus_log)
add_subdirectory(os_freertos_runtime)

add_subdirectory(stm32_model)

nexus_behavior_test(contract_flash_storage SOURCES components_flash_storage.c
    LIBRARIES Nexus::FlashStorage Nexus::NativeModel)
nexus_behavior_test(contract_spi_adapter SOURCES components_spi.c
    LIBRARIES Nexus::BMP280SPI Nexus::SpiOwner Nexus::OSNative Nexus::NativeModel)
nexus_behavior_test(contract_uart_adapter SOURCES components_uart.c
    LIBRARIES Nexus::UARTOwner Nexus::OSNative Nexus::NativeModel)

# New behavioral contracts use one pinned host-only GoogleTest/GoogleMock layer.
add_subdirectory("${NEXUS_SOURCE_DIR}/tests/google" google)
