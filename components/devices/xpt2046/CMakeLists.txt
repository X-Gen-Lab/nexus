# Exact raw touch component; no worker, ADC calibration or panel policy.
# Author: Nexus Team
add_library(nexus_xpt2046 STATIC EXCLUDE_FROM_ALL src/xpt2046.c)
nexus_apply_target_policy(nexus_xpt2046)
target_include_directories(nexus_xpt2046 PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_xpt2046 PUBLIC Nexus::IOSPI Nexus::IOGPIO Nexus::IOEXTI
    PRIVATE nexus_build_options nexus_owned_warnings)
add_library(Nexus::XPT2046 ALIAS nexus_xpt2046)
