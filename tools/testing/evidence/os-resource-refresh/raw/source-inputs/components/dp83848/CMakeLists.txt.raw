# Explicit PHY service; no worker, descriptor or network stack is implicit.
add_library(nexus_dp83848 STATIC src/dp83848.c)
target_include_directories(nexus_dp83848 PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_dp83848 PUBLIC nexus_io nexus_core
    PRIVATE nexus_build_options nexus_owned_warnings)
add_library(Nexus::DP83848 ALIAS nexus_dp83848)
