# Optional coherent PWM and sampling plans; no provider or OS dependency.
# Author: Nexus Team
add_library(
    nexus_modulation_control
    STATIC
    EXCLUDE_FROM_ALL
    src/modulation.c
    src/phase_shunt.c
    src/single_shunt.c
    src/dispatch.c
)
nexus_apply_target_policy(nexus_modulation_control)
target_include_directories(
    nexus_modulation_control
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_modulation_control
    PUBLIC Nexus::MotorControl
    PRIVATE nexus_build_options nexus_owned_warnings
)
if(NOT MSVC)
    target_link_libraries(nexus_modulation_control PRIVATE m)
endif()
add_library(Nexus::ModulationControl ALIAS nexus_modulation_control)
