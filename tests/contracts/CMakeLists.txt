# Explicit behavior suites, never inferred from a recursive file scan.
add_test(NAME contract_configuration_rejection
    COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover
        -s "${NEXUS_SOURCE_DIR}/tools/configure" -p "test_*.py")
set_tests_properties(contract_configuration_rejection PROPERTIES
    WORKING_DIRECTORY "${NEXUS_SOURCE_DIR}")

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

set(gd32_driver_root "${NEXUS_SOURCE_DIR}/soc/gd32f470/drivers")
set(gd32_vendor_root "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
set(gd32_driver_sources)
foreach(unit gpio uart spi i2c flash watchdog exti timer adc)
    list(APPEND gd32_driver_sources "${gd32_driver_root}/${unit}.c")
endforeach()
nexus_behavior_test(contract_gd32_io SOURCES gd32_io_test.c
    gd32_model/model.c ${gd32_driver_sources}
    LIBRARIES Nexus::IO
    INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/gd32_model"
        "${NEXUS_SOURCE_DIR}/soc/gd32f470"
        "${NEXUS_SOURCE_DIR}/soc/gd32f470/private"
    DEFINITIONS GD32F470 HXTAL_VALUE=25000000)
# SDK address macros are uint32_t by design; the host model interprets those
# addresses. This host-only diagnostic differs from the unchanged ARM gate.
target_compile_options(contract_gd32_io PRIVATE -Wno-int-to-pointer-cast)
target_include_directories(contract_gd32_io SYSTEM PRIVATE
    "${gd32_vendor_root}/CMSIS/GD/GD32F4xx/Include"
    "${gd32_vendor_root}/GD32F4xx_standard_peripheral/Include")
nexus_behavior_test(contract_gd32_system SOURCES gd32_system_test.c
    "${NEXUS_SOURCE_DIR}/soc/gd32f470/system.c"
    "${NEXUS_SOURCE_DIR}/soc/gd32f470/timebase.c"
    LIBRARIES Nexus::IO
    INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/gd32_model"
        "${NEXUS_SOURCE_DIR}/soc/gd32f470"
        "${NEXUS_SOURCE_DIR}/soc/gd32f470/private"
    DEFINITIONS GD32F470 HXTAL_VALUE=25000000 NX_GD32_CLOCK_MODEL)
target_compile_options(contract_gd32_system PRIVATE -Wno-int-to-pointer-cast)
target_include_directories(contract_gd32_system SYSTEM PRIVATE
    "${gd32_vendor_root}/CMSIS/GD/GD32F4xx/Include"
    "${gd32_vendor_root}/GD32F4xx_standard_peripheral/Include")
foreach(scenario watchdog-late-failure watchdog-hardware-enabled)
    add_test(NAME contract_gd32_${scenario} COMMAND contract_gd32_io ${scenario})
    set_tests_properties(contract_gd32_${scenario} PROPERTIES TIMEOUT 30)
endforeach()

# New behavioral contracts use one pinned host-only GoogleTest/GoogleMock layer.
add_subdirectory("${NEXUS_SOURCE_DIR}/tests/google" google)
