# Core consumes an explicit OSAL runtime, never HAL or vendor SDK headers.
add_library(log_interface INTERFACE)
target_include_directories(log_interface INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
add_library(log_core STATIC src/log_format.c)
target_link_libraries(log_core PUBLIC log_interface nexus_build_options)
add_library(log_framework STATIC src/log.c)
target_include_directories(log_framework PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(log_framework PUBLIC log_interface log_core nexus_build_options PRIVATE osal)
target_compile_definitions(log_framework PRIVATE $<$<CONFIG:Debug>:LOG_DEBUG_ENABLED>)
# UART integration is selected explicitly by the final application.
add_library(log_uart_adapter STATIC src/log_backend_uart.c)
target_link_libraries(log_uart_adapter PUBLIC log_interface Nexus::HALDevice PRIVATE log_framework osal)
