# Selected AS5047P polling protocol; caller owns electrical timing and wiring.
# Author: Nexus Team
add_library(nexus_as5047p STATIC EXCLUDE_FROM_ALL src/as5047p.c)
nexus_apply_target_policy(nexus_as5047p)
target_include_directories(
    nexus_as5047p
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_as5047p
    PUBLIC Nexus::IOSPI
    PRIVATE nexus_build_options nexus_owned_warnings
)
add_library(Nexus::AS5047P ALIAS nexus_as5047p)
