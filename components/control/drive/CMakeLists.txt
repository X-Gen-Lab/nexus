# Optional explicit current-reference and feedback composition.
# Author: Nexus Team
add_library(nexus_drive_control STATIC EXCLUDE_FROM_ALL src/drive.c)
nexus_apply_target_policy(nexus_drive_control)
target_include_directories(
    nexus_drive_control
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_drive_control
    PUBLIC Nexus::MotorControl Nexus::AxisControl
    PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings
)
add_library(Nexus::DriveControl ALIAS nexus_drive_control)
