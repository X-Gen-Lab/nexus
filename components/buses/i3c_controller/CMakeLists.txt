# Explicit allocation-free SDR controller core; no physical provider selected.
# Author: Nexus Team
add_library(nexus_i3c_controller STATIC EXCLUDE_FROM_ALL src/i3c_controller.c)
target_include_directories(nexus_i3c_controller PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_i3c_controller PUBLIC Nexus::Core
    PRIVATE nexus_build_options nexus_owned_warnings)
add_library(Nexus::I3CController ALIAS nexus_i3c_controller)
