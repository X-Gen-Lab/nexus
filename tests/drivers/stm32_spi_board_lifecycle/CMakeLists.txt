# Can run alone, or be included by the host driver contract suite.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.21)
    project(NexusDiscoverySpiBoardContracts LANGUAGES C)
    enable_testing()
endif()
get_filename_component(NEXUS_SPI_BOARD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
add_executable(driver_stm32_spi_board_lifecycle test_board.c
    "${NEXUS_SPI_BOARD_ROOT}/boards/stm32f4discovery/spi.c")
target_include_directories(driver_stm32_spi_board_lifecycle PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fake"
    "${NEXUS_SPI_BOARD_ROOT}/hal/include"
    "${NEXUS_SPI_BOARD_ROOT}/soc/stm32f407/controllers/spi")
target_compile_features(driver_stm32_spi_board_lifecycle PRIVATE c_std_11)
if(TARGET nexus_build_options)
    target_link_libraries(driver_stm32_spi_board_lifecycle PRIVATE nexus_build_options)
endif()
if(MSVC)
    target_compile_options(driver_stm32_spi_board_lifecycle PRIVATE /UNDEBUG)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(driver_stm32_spi_board_lifecycle PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
foreach(scenario unsupported-preflight manager-preflight polling-idempotent
    irq-disconnect-retry dma-disconnect-retry tx-init-partial rx-init-partial
    connect-partial external-irq-owner handle-owner)
    add_test(NAME "driver_stm32_spi_board_lifecycle.${scenario}"
        COMMAND driver_stm32_spi_board_lifecycle "${scenario}")
    set_tests_properties("driver_stm32_spi_board_lifecycle.${scenario}" PROPERTIES
        TIMEOUT 10 LABELS "board;stm32;spi;host-model;lifecycle;fault")
endforeach()
