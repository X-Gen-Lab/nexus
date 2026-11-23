# Real codec protocol with only external settled I2C and time observations modeled.
# Author: Nexus Team
nexus_google_test(
    nexus_wm8978_test
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/wm8978_test.cpp"
    LIBRARIES Nexus::WM8978
)
