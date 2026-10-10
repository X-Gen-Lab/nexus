# Host-only framework: firmware and source-SDK consumers never enter this tree.
enable_language(CXX)
if(CMAKE_CROSSCOMPILING OR NOT NEXUS_SOC_FAMILY STREQUAL "native")
    message(FATAL_ERROR "GoogleTest/GoogleMock require the Native test profile")
endif()

file(READ "${NEXUS_SOURCE_DIR}/dependencies/googletest.lock.json" google_lock)
string(JSON google_archive GET "${google_lock}" archive)
string(JSON google_commit GET "${google_lock}" commit)
string(JSON google_sha256 GET "${google_lock}" sha256)
set(google_archive "${NEXUS_SOURCE_DIR}/${google_archive}")
if(NOT EXISTS "${google_archive}")
    message(FATAL_ERROR "Missing pinned GoogleTest archive; no network fallback")
endif()
file(SHA256 "${google_archive}" google_actual)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${NEXUS_SOURCE_DIR}/dependencies/googletest.lock.json" "${google_archive}")
if(NOT google_actual STREQUAL google_sha256)
    message(FATAL_ERROR "GoogleTest archive differs from the source lock")
endif()
set(google_root "${CMAKE_BINARY_DIR}/_deps/googletest-${google_commit}")
# A fresh extraction prevents a previously edited dependency from being reused.
file(REMOVE_RECURSE "${google_root}")
file(ARCHIVE_EXTRACT INPUT "${google_archive}"
    DESTINATION "${CMAKE_BINARY_DIR}/_deps")
set(INSTALL_GTEST OFF CACHE BOOL "GoogleTest is test-only" FORCE)
set(BUILD_GMOCK ON CACHE BOOL "GoogleMock contract tests" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "Host MSVC runtime" FORCE)
add_subdirectory("${google_root}" "${CMAKE_BINARY_DIR}/_deps/google-build"
    EXCLUDE_FROM_ALL)

add_custom_target(nexus_google_contracts)
function(nexus_google_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBRARIES;INCLUDES" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    target_compile_features(${name} PRIVATE cxx_std_17)
    set_target_properties(${name} PROPERTIES CXX_EXTENSIONS OFF)
    target_link_libraries(${name} PRIVATE nexus_build_options nexus_owned_warnings
        GTest::gmock_main ${ARG_LIBRARIES})
    target_include_directories(${name} PRIVATE ${ARG_INCLUDES})
    add_dependencies(nexus_google_contracts ${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 30 LABELS "GoogleTest")
    set_property(GLOBAL APPEND PROPERTY NEXUS_GOOGLE_EXECUTABLES
        "$<TARGET_FILE:${name}>")
endfunction()

nexus_google_test(nexus_owner_mock_test SOURCES owner_mock_test.cpp
    LIBRARIES Nexus::BusOwner)
nexus_google_test(nexus_realtime_metadata_test SOURCES realtime_metadata_test.cpp
    LIBRARIES Nexus::BusOwner Nexus::IO Nexus::Arch)
nexus_google_test(nexus_polymorphic_io_test SOURCES polymorphic_mock_test.cpp
    LIBRARIES Nexus::IO)
nexus_google_test(nexus_uart_owner_mock_test SOURCES uart_owner_test.cpp
    LIBRARIES Nexus::UARTOwner)
nexus_google_test(nexus_spi_async_owner_mock_test SOURCES spi_async_owner_test.cpp
    LIBRARIES Nexus::SPIAsyncOwner)
nexus_google_test(nexus_hardware_contract_test
    SOURCES dma_test.cpp stream_test.cpp wake_test.cpp
    LIBRARIES Nexus::IO Nexus::Arch)
nexus_google_test(nexus_stream_storage_test SOURCES stream_storage_test.cpp
    LIBRARIES Nexus::IO Nexus::Arch)
nexus_google_test(nexus_native_instances_test SOURCES native_instances_test.cpp
    LIBRARIES Nexus::NativeModel
    INCLUDES "${NEXUS_SOURCE_DIR}/io/native")
nexus_google_test(nexus_native_async_test SOURCES native_async_test.cpp
    LIBRARIES Nexus::NativeModel
    INCLUDES "${NEXUS_SOURCE_DIR}/io/native")
nexus_google_test(nexus_irq_wake_provider_test SOURCES irq_wake_test.cpp
    LIBRARIES Nexus::NativeModel
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

nexus_google_test(nexus_stm32_uart_dma_test SOURCES stm32_uart_dma_test.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/uart.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32f407/private")
target_include_directories(nexus_stm32_uart_dma_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(nexus_stm32_uart_dma_test PRIVATE
    NEXUS_STM32_MODEL STM32F407xx)

target_sources(nexus_stm32_uart_dma_test PRIVATE
    "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/uart_dma.c")

target_compile_options(nexus_stm32_uart_dma_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_dma_model.h")

# The host model compiles production register algorithms for two GD controllers.
set(gd_multi_root "${NEXUS_SOURCE_DIR}/soc/gd32f470")
set(gd_multi_vendor "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
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

nexus_google_test(nexus_stm32_spi_dma_test
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

nexus_google_test(nexus_stm32_uart_stream_test
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
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
target_include_directories(nexus_gd32_spi_dma_test SYSTEM PRIVATE
    "${gd_multi_vendor}/CMSIS/GD/GD32F4xx/Include"
    "${gd_multi_vendor}/GD32F4xx_standard_peripheral/Include")
target_compile_definitions(nexus_gd32_spi_dma_test PRIVATE GD32F470
    HXTAL_VALUE=25000000)
target_compile_options(nexus_gd32_spi_dma_test PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>"
    "$<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/gd32_spi_dma_model.h>")

nexus_google_test(nexus_stm32_adc_stream_test
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
    "${NEXUS_SOURCE_DIR}/soc/stm32f407/drivers/adc_stream.c")

nexus_google_test(nexus_gd32_adc_stream_test SOURCES gd32_adc_stream_test.cpp
    "${gd_multi_root}/drivers/adc.c" "${gd_multi_root}/drivers/adc_stream.c"
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

get_property(google_executables GLOBAL PROPERTY NEXUS_GOOGLE_EXECUTABLES)
list(JOIN google_executables "\",\n    \"" google_executables)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/google-contracts.json"
    CONTENT "{\n  \"schema\": 1,\n  \"executables\": [\n    \"${google_executables}\"\n  ]\n}\n")
