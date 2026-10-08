# Production core with fault-capable driver ports and the real Native Arch lock.
add_executable(typed_device_contract_tests
    test_device.cpp test_spi.cpp "${NEXUS_SOURCE_DIR}/hal/src/nx_device.c"
    "${NEXUS_SOURCE_DIR}/hal/src/nx_device_spi.c")
# Override only the ISR context probe in this fault-model executable.
# The production library uses Arch IPSR; locking/barriers use real Native Arch.
target_compile_definitions(typed_device_contract_tests PRIVATE nx_arch_in_isr=typed_test_in_isr)
target_compile_features(typed_device_contract_tests PRIVATE cxx_std_17)
target_link_libraries(typed_device_contract_tests PRIVATE
    hal_interface nexus_arch GTest::gtest_main ${THREAD_LIBS})
if(NOT MSVC)
    target_compile_options(typed_device_contract_tests PRIVATE -Wall -Wextra -Werror)
endif()
nexus_discover_tests(typed_device_contract_tests PROPERTIES TIMEOUT 10 LABELS "hal;typed;contract")

add_executable(typed_stm32_gpio_registration
    test_stm32_gpio_registration.c "${NEXUS_SOURCE_DIR}/hal/src/nx_device.c"
    "${NEXUS_SOURCE_DIR}/platforms/stm32/src/hal/gpio/stm32_gpio_device.c"
    "${NEXUS_SOURCE_DIR}/platforms/stm32/src/hal/gpio/stm32_gpio_lifecycle.c"
    "${NEXUS_SOURCE_DIR}/platforms/stm32/src/hal/gpio/stm32_gpio_power.c"
    "${NEXUS_SOURCE_DIR}/platforms/stm32/src/hal/gpio/stm32_gpio_ops.c")
target_include_directories(typed_stm32_gpio_registration PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fake"
    "${NEXUS_SOURCE_DIR}/platforms/stm32/src/hal/gpio")
target_compile_definitions(typed_stm32_gpio_registration PRIVATE STM32F4 NEXUS_PLATFORM_NATIVE)
target_link_libraries(typed_stm32_gpio_registration PRIVATE hal_interface nexus_arch)
if(MSVC)
    target_compile_options(typed_stm32_gpio_registration PRIVATE /UNDEBUG)
else()
    target_compile_options(typed_stm32_gpio_registration PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME typed_stm32_gpio_registration COMMAND typed_stm32_gpio_registration)
set_tests_properties(typed_stm32_gpio_registration PROPERTIES TIMEOUT 10 LABELS "hal;typed;driver;host-fake;contract")

# Actual selected Native provider and Product assembly, without ISR overrides.
add_executable(typed_native_spi_tests test_native_spi.cpp
    "${NEXUS_SOURCE_DIR}/tests/hal/native/devices/native_spi_helpers.c")
target_compile_features(typed_native_spi_tests PRIVATE cxx_std_17)
target_include_directories(typed_native_spi_tests PRIVATE
    "${NEXUS_SOURCE_DIR}/tests/hal/native")
target_link_libraries(typed_native_spi_tests PRIVATE Nexus::Product GTest::gtest_main ${THREAD_LIBS})
if(NOT MSVC)
    target_compile_options(typed_native_spi_tests PRIVATE -Wall -Wextra -Werror)
endif()
nexus_discover_tests(typed_native_spi_tests PROPERTIES TIMEOUT 10 LABELS "hal;typed;native;product;contract")
