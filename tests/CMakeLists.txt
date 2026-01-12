##############################################################################
# Nexus Test Framework
##############################################################################

cmake_minimum_required(VERSION 3.16)

# Enable testing
enable_testing()

# Fetch Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Include Google Test
include(GoogleTest)

# Test executable
add_executable(nexus_tests
    test_main.cpp
    hal/test_hal_gpio.cpp
    hal/test_hal_gpio_properties.cpp
    hal/test_hal_uart.cpp
    hal/test_hal_uart_properties.cpp
    hal/test_hal_spi.cpp
    hal/test_hal_spi_properties.cpp
    hal/test_hal_i2c.cpp
    hal/test_hal_i2c_properties.cpp
)

# Require C++20 for designated initializers
target_compile_features(nexus_tests PRIVATE cxx_std_20)

target_include_directories(nexus_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/hal/include
        ${CMAKE_SOURCE_DIR}/platforms/native/include
)

target_link_libraries(nexus_tests
    PRIVATE
        GTest::gtest
        GTest::gmock
        platform_native
        hal_interface
)

# Discover tests
gtest_discover_tests(nexus_tests)
