# Production core with fault-capable driver ports and the real Native Arch lock.
add_executable(typed_device_contract_tests
    test_device.cpp test_spi.cpp test_flash.cpp "${NEXUS_SOURCE_DIR}/hal/src/nx_device.c"
    "${NEXUS_SOURCE_DIR}/hal/src/nx_device_spi.c"
    "${NEXUS_SOURCE_DIR}/hal/src/nx_device_gpio.c"
    "${NEXUS_SOURCE_DIR}/hal/src/nx_device_uart.c"
    "${NEXUS_SOURCE_DIR}/hal/src/nx_device_flash.c"
    "${NEXUS_SOURCE_DIR}/hal/src/nx_device_i2c.c"
    "${NEXUS_SOURCE_DIR}/hal/src/nx_deadline.c")
# Override only the ISR context probe in this fault-model executable.
# The production library uses Arch IPSR; locking/barriers use real Native Arch.
target_compile_definitions(typed_device_contract_tests PRIVATE nx_arch_in_isr=typed_test_in_isr)
target_compile_features(typed_device_contract_tests PRIVATE cxx_std_17)
target_link_libraries(typed_device_contract_tests PRIVATE
    hal_provider_interface nexus_arch GTest::gtest_main ${THREAD_LIBS})
if(NOT MSVC)
    target_compile_options(typed_device_contract_tests PRIVATE -Wall -Wextra -Werror)
endif()
nexus_discover_tests(typed_device_contract_tests PROPERTIES TIMEOUT 10 LABELS "hal;typed;contract")

add_executable(typed_stm32_gpio_registration
    test_stm32_gpio_registration.c "${NEXUS_SOURCE_DIR}/hal/src/nx_device.c"
    "${NEXUS_SOURCE_DIR}/hal/src/nx_device_gpio.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32f407/controllers/gpio/stm32_gpio_device.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32f407/controllers/gpio/stm32_gpio_lifecycle.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32f407/controllers/gpio/stm32_gpio_power.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32f407/controllers/gpio/stm32_gpio_ops.c")
target_include_directories(typed_stm32_gpio_registration PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fake"
    "${NEXUS_SOURCE_DIR}/soc/stm32f407/controllers/gpio")
target_compile_definitions(typed_stm32_gpio_registration PRIVATE STM32F4 NEXUS_PLATFORM_NATIVE)
target_link_libraries(typed_stm32_gpio_registration PRIVATE hal_provider_interface nexus_arch)
if(MSVC)
    target_compile_options(typed_stm32_gpio_registration PRIVATE /UNDEBUG)
else()
    target_compile_options(typed_stm32_gpio_registration PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME typed_stm32_gpio_registration COMMAND typed_stm32_gpio_registration)
set_tests_properties(typed_stm32_gpio_registration PROPERTIES TIMEOUT 10 LABELS "hal;typed;driver;host-fake;contract")

# Actual selected Native provider and Runtime assembly, without ISR overrides.
add_executable(typed_native_spi_tests test_native_spi.cpp
    "${NEXUS_SOURCE_DIR}/tests/hal/native/devices/native_spi_helpers.c")
target_compile_features(typed_native_spi_tests PRIVATE cxx_std_17)
target_include_directories(typed_native_spi_tests PRIVATE
    "${NEXUS_SOURCE_DIR}/tests/hal/native")
target_link_libraries(typed_native_spi_tests PRIVATE Nexus::Firmware Nexus::HALProviders GTest::gtest_main ${THREAD_LIBS})
if(NOT MSVC)
    target_compile_options(typed_native_spi_tests PRIVATE -Wall -Wextra -Werror)
endif()
nexus_discover_tests(typed_native_spi_tests PROPERTIES TIMEOUT 10 LABELS "hal;typed;native;runtime;contract")

# Actual Native I2C provider; injection ports are explicit test-only sources.
add_executable(typed_native_i2c_tests test_native_i2c.cpp native_i2c_fixture.c)
target_compile_features(typed_native_i2c_tests PRIVATE cxx_std_17)
target_link_libraries(typed_native_i2c_tests PRIVATE Nexus::Firmware Nexus::HALProviders Nexus::HALRuntimeOSAL GTest::gtest_main ${THREAD_LIBS})
if(NOT MSVC)
    target_compile_options(typed_native_i2c_tests PRIVATE -Wall -Wextra -Werror)
endif()
nexus_discover_tests(typed_native_i2c_tests PROPERTIES TIMEOUT 10 LABELS "hal;typed;i2c;native;runtime;contract")

add_executable(typed_hal_minimal_link test_minimal_hal.c)
target_link_libraries(typed_hal_minimal_link PRIVATE Nexus::HALGPIO)
add_test(NAME typed_hal_minimal_link COMMAND typed_hal_minimal_link)
set_tests_properties(typed_hal_minimal_link PROPERTIES TIMEOUT 10 LABELS "hal;typed;minimal;contract")

# Compile the ordinary consumer umbrella: mutable provider state must stay opaque.
# CMake's real compiler checks are portable across GCC/Clang/MSVC generators.
include(CheckCSourceCompiles)
set(_hal_saved_required_includes "${CMAKE_REQUIRED_INCLUDES}")
set(_hal_saved_try_type "${CMAKE_TRY_COMPILE_TARGET_TYPE}")
set(CMAKE_REQUIRED_INCLUDES "${NEXUS_SOURCE_DIR}/hal/include;${NEXUS_CONFIG_DIR}")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
unset(NEXUS_HAL_MUTABLE_DESCRIPTOR_VISIBLE CACHE)
check_c_source_compiles("#include <hal/nx_hal.h>\nvoid probe(nx_device_ref_t r) { (void)r.descriptor->state; }"
    NEXUS_HAL_MUTABLE_DESCRIPTOR_VISIBLE)
if(NEXUS_HAL_MUTABLE_DESCRIPTOR_VISIBLE)
    message(FATAL_ERROR "Consumer HAL header exposed mutable provider descriptor state")
endif()
set(CMAKE_REQUIRED_INCLUDES "${_hal_saved_required_includes}")
if(_hal_saved_try_type)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE "${_hal_saved_try_type}")
else()
    unset(CMAKE_TRY_COMPILE_TARGET_TYPE)
endif()
