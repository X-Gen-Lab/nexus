# Explicit optional shared-memory mechanism; no SoC or hardware port selected.
# Author: Nexus Team
add_library(nexus_amp_ipc STATIC EXCLUDE_FROM_ALL src/amp_ipc.c)
target_include_directories(nexus_amp_ipc PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_amp_ipc PUBLIC Nexus::Core
    PRIVATE nexus_build_options nexus_owned_warnings)
add_library(Nexus::AMPIPC ALIAS nexus_amp_ipc)
