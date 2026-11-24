# Real C11 motor calculations have no hardware or OS test substitute.
# Author: Nexus Team
nexus_google_test(nexus_motor_control_test
    SOURCES "${nexus_google_root}/components/motor_control_test.cpp"
    LIBRARIES Nexus::MotorControl
)
