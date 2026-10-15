# Optional privileged gateway mechanism; no default instance or user-mode entry.
# Author: Nexus Team
add_library(nexus_user_io_broker STATIC EXCLUDE_FROM_ALL src/user_io_broker.c)
target_include_directories(nexus_user_io_broker PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/private")
target_link_libraries(nexus_user_io_broker PUBLIC Nexus::Core Nexus::IO
    PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
add_library(Nexus::UserIoBroker ALIAS nexus_user_io_broker)
