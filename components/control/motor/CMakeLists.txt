# Optional pure C11 current control; no device, OS or allocation dependency.
# Author: Nexus Team
add_library(nexus_motor_control STATIC EXCLUDE_FROM_ALL src/motor.c)
nexus_apply_target_policy(nexus_motor_control)
target_include_directories(
    nexus_motor_control
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_motor_control
    PUBLIC Nexus::Core
    PRIVATE nexus_build_options nexus_owned_warnings
)
if(NOT MSVC)
    target_link_libraries(nexus_motor_control PRIVATE m)
endif()
add_library(Nexus::MotorControl ALIAS nexus_motor_control)
