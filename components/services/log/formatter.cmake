# Optional record formatting and copied UART staging reuse the existing Log port.
# Author: Nexus Team
add_library(
    nexus_log_formatter
    STATIC
    EXCLUDE_FROM_ALL
    "${CMAKE_CURRENT_LIST_DIR}/src/log_format.c"
)
target_include_directories(
    nexus_log_formatter
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/private"
)
target_link_libraries(
    nexus_log_formatter
    PUBLIC nexus_log
    PRIVATE nexus_owned_warnings nexus_build_options
)
add_library(Nexus::LogFormatter ALIAS nexus_log_formatter)
nexus_apply_target_policy(nexus_log_formatter)

add_library(
    nexus_log_uart
    STATIC
    EXCLUDE_FROM_ALL
    "${CMAKE_CURRENT_LIST_DIR}/src/log_uart.c"
)
target_link_libraries(
    nexus_log_uart
    PUBLIC nexus_log nexus_io_uart
    PRIVATE nexus_arch nexus_owned_warnings nexus_build_options
)
add_library(Nexus::LogUART ALIAS nexus_log_uart)
nexus_apply_target_policy(nexus_log_uart)
