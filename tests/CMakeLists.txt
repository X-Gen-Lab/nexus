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
    hal/test_build_system.cpp
    hal/test_compiler_abstraction.cpp
    hal/test_hal_gpio.cpp
    hal/test_hal_gpio_properties.cpp
    hal/test_hal_uart.cpp
    hal/test_hal_uart_properties.cpp
    hal/test_hal_spi.cpp
    hal/test_hal_spi_properties.cpp
    hal/test_hal_i2c.cpp
    hal/test_hal_i2c_properties.cpp
    hal/test_hal_timer.cpp
    hal/test_hal_timer_properties.cpp
    hal/test_hal_adc.cpp
    hal/test_hal_adc_properties.cpp
    hal/test_hal_system_properties.cpp
    hal/test_hal_error_handling_properties.cpp
    osal/test_osal_task.cpp
    osal/test_osal_task_properties.cpp
    osal/test_osal_mutex.cpp
    osal/test_osal_mutex_properties.cpp
    osal/test_osal_sem.cpp
    osal/test_osal_sem_properties.cpp
    osal/test_osal_queue.cpp
    osal/test_osal_queue_properties.cpp
    osal/test_osal_timer.cpp
    osal/test_osal_timer_properties.cpp
    osal/test_osal_mem.cpp
    osal/test_osal_mem_properties.cpp
    osal/test_osal_event.cpp
    osal/test_osal_event_properties.cpp
    osal/test_osal_core_properties.cpp
    osal/test_osal_error_handling_properties.cpp
    log/test_log.cpp
    log/test_log_properties.cpp
    log/test_log_uart_backend.cpp
    shell/test_shell_parser.cpp
    shell/test_shell_parser_properties.cpp
    shell/test_shell_line_editor.cpp
    shell/test_shell_line_editor_properties.cpp
    shell/test_shell_history.cpp
    shell/test_shell_history_properties.cpp
    shell/test_shell_autocomplete.cpp
    shell/test_shell_autocomplete_properties.cpp
    shell/test_shell_command.cpp
    shell/test_shell_command_properties.cpp
    shell/test_shell_core.cpp
    shell/test_shell_core_properties.cpp
    shell/test_shell_backend.cpp
    shell/test_shell_backend_properties.cpp
    shell/test_shell_builtin.cpp
    config/test_config_store.cpp
    config/test_config_store_properties.cpp
    config/test_config_core_properties.cpp
    config/test_config_default.cpp
    config/test_config_default_properties.cpp
    config/test_config_namespace.cpp
    config/test_config_namespace_properties.cpp
    config/test_config_callback.cpp
    config/test_config_callback_properties.cpp
    config/test_config_query.cpp
    config/test_config_backend.cpp
    config/test_config_backend_properties.cpp
    config/test_config_import_export.cpp
    config/test_config_import_export_properties.cpp
    config/test_config_crypto.cpp
    config/test_config_crypto_properties.cpp
    integration/test_hal_osal_integration.cpp
    integration/test_freertos_adapter_integration.cpp
    integration/test_log_integration.cpp
    integration/test_shell_integration.cpp
    integration/test_config_integration.cpp
)

# Require C++20 for designated initializers
target_compile_features(nexus_tests PRIVATE cxx_std_20)

target_include_directories(nexus_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/hal/include
        ${CMAKE_SOURCE_DIR}/osal/include
        ${CMAKE_SOURCE_DIR}/platforms/native/include
        ${CMAKE_SOURCE_DIR}/platforms/stm32f4/include
        ${CMAKE_SOURCE_DIR}/framework/log/include
        ${CMAKE_SOURCE_DIR}/framework/shell/include
        ${CMAKE_SOURCE_DIR}/framework/config/include
        ${CMAKE_SOURCE_DIR}
)

# Platform-specific threading libraries
if(WIN32)
    set(THREAD_LIBS "")
else()
    find_package(Threads REQUIRED)
    set(THREAD_LIBS Threads::Threads)
endif()

target_link_libraries(nexus_tests
    PRIVATE
        GTest::gtest
        GTest::gmock
        platform_native
        hal_interface
        hal
        osal
        log_framework
        shell_framework
        config_framework
        ${THREAD_LIBS}
)

# Discover tests
gtest_discover_tests(nexus_tests)
