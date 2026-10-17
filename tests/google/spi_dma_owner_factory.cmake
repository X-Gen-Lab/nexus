# Ordinary authored assembly with two real Native enhanced controllers.
set(spi_owner_factory_input
    "${NEXUS_SOURCE_DIR}/tests/contracts/io_evolution_assembly/native-spi-dma-transaction.toml")
set(spi_owner_factory_bundle "${CMAKE_CURRENT_BINARY_DIR}/native-spi-dma-factory")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${spi_owner_factory_input}")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
    "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
    --source-root "${NEXUS_SOURCE_DIR}" --assembly "${spi_owner_factory_input}"
    --output "${spi_owner_factory_bundle}" RESULT_VARIABLE spi_owner_factory_result
    OUTPUT_VARIABLE spi_owner_factory_output ERROR_VARIABLE spi_owner_factory_error)
if(NOT spi_owner_factory_result EQUAL 0)
    message(FATAL_ERROR "Native SPI DMA owner fixture rejected: "
        "${spi_owner_factory_output}${spi_owner_factory_error}")
endif()
file(READ "${spi_owner_factory_bundle}/input_paths.json" spi_owner_factory_inputs)
string(JSON spi_owner_factory_count LENGTH "${spi_owner_factory_inputs}")
math(EXPR spi_owner_factory_last "${spi_owner_factory_count} - 1")
foreach(index RANGE 0 ${spi_owner_factory_last})
    string(JSON spi_owner_factory_name MEMBER "${spi_owner_factory_inputs}" ${index})
    string(JSON spi_owner_factory_path GET "${spi_owner_factory_inputs}"
        "${spi_owner_factory_name}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${spi_owner_factory_path}")
endforeach()
nexus_google_test(nexus_native_spi_dma_factory_test
    SOURCES native_spi_dma_factory_test.cpp
        "${spi_owner_factory_bundle}/bindings.c"
    LIBRARIES Nexus::SPIDMATransactionOwner Nexus::NativeModel Nexus::Arch
    INCLUDES "${spi_owner_factory_bundle}" "${NEXUS_SOURCE_DIR}/io/native"
        "${NEXUS_SOURCE_DIR}/io/private")
