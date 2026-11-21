# CRC-128 receive framing; transport and persistence policy remain explicit.
# Author: Nexus Team
add_library(nexus_xmodem STATIC EXCLUDE_FROM_ALL src/xmodem.c)
target_include_directories(
    nexus_xmodem
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_xmodem
    PUBLIC Nexus::Core
    PRIVATE Nexus::Algorithms nexus_build_options nexus_owned_warnings
)
add_library(Nexus::XModem ALIAS nexus_xmodem)
nexus_apply_target_policy(nexus_xmodem)
