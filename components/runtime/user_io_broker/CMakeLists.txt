# Optional privileged gateways; no default instance or user-mode entry.
# Author: Nexus Team
add_library(nexus_user_io_broker STATIC EXCLUDE_FROM_ALL src/user_io_broker.c)
target_include_directories(
    nexus_user_io_broker
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/private"
)
target_link_libraries(
    nexus_user_io_broker
    PUBLIC Nexus::Core Nexus::IOGPIO
    PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings
)
add_library(Nexus::UserIoBroker ALIAS nexus_user_io_broker)

add_library(nexus_user_io_async STATIC EXCLUDE_FROM_ALL src/user_io_async.c)
target_include_directories(
    nexus_user_io_async
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_user_io_async
    PUBLIC Nexus::Core Nexus::UARTOwner
    PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings
)
add_library(Nexus::UserIoAsync ALIAS nexus_user_io_async)
