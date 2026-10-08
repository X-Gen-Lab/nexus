# Standalone host tests. Production STM32 sources, test-only vendor HAL.
cmake_minimum_required(VERSION 3.21)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(nexus_driver_contracts C)
    enable_testing()
endif()
get_filename_component(NEXUS_DRIVER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(STM32_SPI_SOURCES
    stm32_spi_device.c stm32_spi_sync.c stm32_spi_async.c
    stm32_spi_dma.c stm32_spi_lifecycle.c stm32_spi_power.c)
list(TRANSFORM STM32_SPI_SOURCES PREPEND "${NEXUS_DRIVER_ROOT}/platforms/stm32/src/hal/spi/")
foreach(mode bare osal)
    add_executable(test_driver_stm32_spi_${mode} test_stm32_spi.c ${STM32_SPI_SOURCES})
    target_include_directories(test_driver_stm32_spi_${mode} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/fake"
        "${NEXUS_DRIVER_ROOT}/hal/include"
        "${NEXUS_DRIVER_ROOT}/osal/include"
        "${NEXUS_DRIVER_ROOT}/platforms/stm32/src/hal/spi")
    target_compile_definitions(test_driver_stm32_spi_${mode} PRIVATE STM32F4xx)
    if(mode STREQUAL "osal")
        target_compile_definitions(test_driver_stm32_spi_${mode} PRIVATE NX_CONFIG_STM32_SPI_USE_OSAL=1)
    endif()
    if(TARGET nexus_build_options)
        target_link_libraries(test_driver_stm32_spi_${mode} PRIVATE nexus_build_options)
    endif()
    target_compile_features(test_driver_stm32_spi_${mode} PRIVATE c_std_11)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(test_driver_stm32_spi_${mode} PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME driver_stm32_spi_${mode} COMMAND test_driver_stm32_spi_${mode})
    set_tests_properties(driver_stm32_spi_${mode} PROPERTIES LABELS "driver;host-fake;contract")
endforeach()
add_executable(test_driver_stm32_gpio test_stm32_gpio.c
    "${NEXUS_DRIVER_ROOT}/platforms/stm32/src/hal/gpio/stm32_gpio_lifecycle.c"
    "${NEXUS_DRIVER_ROOT}/platforms/stm32/src/hal/gpio/stm32_gpio_power.c"
    "${NEXUS_DRIVER_ROOT}/platforms/stm32/src/hal/gpio/stm32_gpio_ops.c")
target_include_directories(test_driver_stm32_gpio PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fake" "${NEXUS_DRIVER_ROOT}/hal/include"
    "${NEXUS_DRIVER_ROOT}/platforms/stm32/src/hal/gpio")
target_compile_definitions(test_driver_stm32_gpio PRIVATE STM32F4)
if(TARGET nexus_build_options)
    target_link_libraries(test_driver_stm32_gpio PRIVATE nexus_build_options)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_driver_stm32_gpio PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME driver_stm32_gpio COMMAND test_driver_stm32_gpio)
set_tests_properties(driver_stm32_gpio PROPERTIES LABELS "driver;host-fake;contract")
