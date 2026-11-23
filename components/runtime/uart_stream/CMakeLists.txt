# A stable request bridges borrowed UART TX to immediate copying bytes.
# Author: Nexus Team
add_library(nexus_uart_byte_stream STATIC EXCLUDE_FROM_ALL src/uart_stream.c)
nexus_apply_target_policy(nexus_uart_byte_stream)
target_include_directories(
    nexus_uart_byte_stream
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../protocols/network/include"
)
target_link_libraries(
    nexus_uart_byte_stream
    PUBLIC Nexus::IOUART
    PRIVATE nexus_build_options nexus_owned_warnings
)
add_library(Nexus::UARTByteStream ALIAS nexus_uart_byte_stream)
