# Scalar algorithms are opt-in and retain no execution/OS state.
# Author: Nexus Team
add_library(
    nexus_algorithms
    STATIC
    EXCLUDE_FROM_ALL
    src/numeric.c
    src/crc.c
    src/endian.c
    src/statistics.c
    src/filter.c
    src/interpolation.c
    src/control.c
    src/pid.c
    src/dsp.c
)
nexus_apply_target_policy(nexus_algorithms)
target_include_directories(
    nexus_algorithms
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_algorithms
    PUBLIC Nexus::Core
    PRIVATE nexus_build_options nexus_owned_warnings
)
if(NOT MSVC)
    target_link_libraries(nexus_algorithms PRIVATE m)
endif()
add_library(Nexus::Algorithms ALIAS nexus_algorithms)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmsis_dsp/CMSISDSP.cmake")
