# Standalone host tests. Production STM32 sources, test-only vendor HAL.
cmake_minimum_required(VERSION 3.21)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(nexus_driver_contracts C)
    enable_testing()
endif()

get_filename_component(NEXUS_DRIVER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(gd32f470)
endif()
set(STM32_SPI_SOURCES
    stm32_spi_device.c stm32_spi_sync.c stm32_spi_async.c
    stm32_spi_dma.c stm32_spi_lifecycle.c stm32_spi_power.c)
list(TRANSFORM STM32_SPI_SOURCES PREPEND "${NEXUS_DRIVER_ROOT}/platforms/stm32/src/hal/spi/")
foreach(mode bare osal)
    add_executable(test_driver_stm32_spi_${mode} test_stm32_spi.c ${STM32_SPI_SOURCES})
    target_include_directories(test_driver_stm32_spi_${mode} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/fake"
        "${NEXUS_DRIVER_ROOT}/hal/include"
        "${NEXUS_DRIVER_ROOT}/arch/include"
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

file(GLOB STM32_UART_SOURCES CONFIGURE_DEPENDS
    "${NEXUS_DRIVER_ROOT}/platforms/stm32/src/hal/uart/*.c")
add_executable(test_driver_stm32_uart test_stm32_uart.c ${STM32_UART_SOURCES})
target_include_directories(test_driver_stm32_uart PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fake/uart" "${CMAKE_CURRENT_SOURCE_DIR}/fake"
    "${NEXUS_DRIVER_ROOT}/hal/include" "${NEXUS_DRIVER_ROOT}/osal/include"
    "${NEXUS_DRIVER_ROOT}/arch/include"
    "${NEXUS_DRIVER_ROOT}/platforms/stm32/src/hal/uart")
target_compile_definitions(test_driver_stm32_uart PRIVATE STM32F4xx NEXUS_PLATFORM_NATIVE)
if(TARGET nexus_build_options)
    target_link_libraries(test_driver_stm32_uart PRIVATE nexus_build_options)
endif()
target_compile_features(test_driver_stm32_uart PRIVATE c_std_11)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_driver_stm32_uart PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME driver_stm32_uart COMMAND test_driver_stm32_uart)
set_tests_properties(driver_stm32_uart PROPERTIES LABELS "driver;host-fake;contract")

add_executable(test_driver_stm32_uart_clock test_stm32_uart_clock.c
    "${NEXUS_DRIVER_ROOT}/soc/stm32f407vg/stm32_uart_clock.c")
target_include_directories(test_driver_stm32_uart_clock PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fake" "${NEXUS_DRIVER_ROOT}/hal/include"
    "${NEXUS_DRIVER_ROOT}/arch/include"
    "${NEXUS_DRIVER_ROOT}/platforms/stm32/src/hal/uart")
target_compile_definitions(test_driver_stm32_uart_clock PRIVATE
    NEXUS_PLATFORM_NATIVE NEXUS_TEST_UART_CLOCK)
if(TARGET nexus_build_options)
    target_link_libraries(test_driver_stm32_uart_clock PRIVATE nexus_build_options)
endif()
target_compile_features(test_driver_stm32_uart_clock PRIVATE c_std_11)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_driver_stm32_uart_clock PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME driver_stm32_uart_clock COMMAND test_driver_stm32_uart_clock)
set_tests_properties(driver_stm32_uart_clock PROPERTIES LABELS "driver;host-fake;contract")

add_executable(test_driver_stm32_flash_geometry test_stm32_flash_geometry.c)
if(TARGET nexus_build_options)
    target_link_libraries(test_driver_stm32_flash_geometry PRIVATE nexus_build_options)
endif()
target_include_directories(test_driver_stm32_flash_geometry PRIVATE
    "${NEXUS_DRIVER_ROOT}/soc/stm32f407vg")
target_compile_features(test_driver_stm32_flash_geometry PRIVATE c_std_11)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_driver_stm32_flash_geometry PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME driver_stm32_flash_geometry COMMAND test_driver_stm32_flash_geometry)
set_tests_properties(driver_stm32_flash_geometry PROPERTIES LABELS "driver;geometry;contract")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    foreach(variant 512k 1m mismatched)
        set(target "test_driver_stm32_flash_${variant}")
        add_executable(${target} test_stm32_flash_port.c
            "${NEXUS_DRIVER_ROOT}/soc/stm32f407vg/flash.c")
        if(TARGET nexus_build_options)
            target_link_libraries(${target} PRIVATE nexus_build_options)
        endif()
        target_include_directories(${target} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/fake" "${NEXUS_DRIVER_ROOT}/hal/include"
            "${NEXUS_DRIVER_ROOT}/arch/include" "${NEXUS_DRIVER_ROOT}/services/storage/include"
            "${NEXUS_DRIVER_ROOT}/soc/stm32f407vg")
        if(variant STREQUAL "1m")
            target_compile_definitions(${target} PRIVATE NX_CONFIG_STM32_FLASH_SIZE=0x100000)
        else()
            target_compile_definitions(${target} PRIVATE NX_CONFIG_STM32_FLASH_SIZE=0x80000)
        endif()
        if(variant STREQUAL "mismatched")
            target_compile_definitions(${target} PRIVATE NEXUS_TEST_MISMATCH_LAYOUT=1)
        endif()
        target_compile_features(${target} PRIVATE c_std_11)
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -UNDEBUG)
        target_link_options(${target} PRIVATE -no-pie)
        add_test(NAME "driver_stm32_flash_${variant}" COMMAND ${target})
        set_tests_properties("driver_stm32_flash_${variant}" PROPERTIES
            LABELS "driver;host-fake;storage;contract" TIMEOUT 10)
    endforeach()
endif()

# Production STM32 IRQ regression with real CMSIS device enums.

add_executable(test_driver_stm32_isr_manager test_stm32_isr_manager.c
    "${NEXUS_DRIVER_ROOT}/platforms/stm32/src/interrupt/stm32_isr_manager.c")
target_include_directories(test_driver_stm32_isr_manager PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fake/interrupt" "${CMAKE_CURRENT_SOURCE_DIR}/fake"
    "${NEXUS_DRIVER_ROOT}/hal/include" "${NEXUS_DRIVER_ROOT}/platforms/stm32/private")
target_include_directories(test_driver_stm32_isr_manager SYSTEM PRIVATE
    "${NEXUS_DRIVER_ROOT}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_DRIVER_ROOT}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
target_compile_definitions(test_driver_stm32_isr_manager PRIVATE STM32F407xx STM32F4)
target_compile_features(test_driver_stm32_isr_manager PRIVATE c_std_11)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_driver_stm32_isr_manager PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME test_driver_stm32_isr_manager COMMAND test_driver_stm32_isr_manager)
set_tests_properties(test_driver_stm32_isr_manager PROPERTIES LABELS "driver;host-fake;contract")
