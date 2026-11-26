# Real upstream and selected sensor/fusion contracts.
# Author: Nexus Team
include("${NEXUS_SOURCE_DIR}/components/devices/lsm6dsox/CMakeLists.txt")
nexus_add_lsm6dsox()
nexus_google_test(nexus_lsm6dsox_test
    SOURCES "${nexus_google_root}/components/devices/lsm6dsox_test.cpp"
    LIBRARIES Nexus::LSM6DSOX Nexus::LSM6DSOXI2C Nexus::LSM6DSOXSPI
)
nexus_google_test(nexus_common_attitude_test
    SOURCES "${nexus_google_root}/components/algorithms/common_attitude_test.cpp"
    LIBRARIES Nexus::Algorithms
)
