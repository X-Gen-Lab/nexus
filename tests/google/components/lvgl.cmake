# Real supplier render, caller storage, failure and final withdrawal.
# Author: Nexus Team
nexus_add_lvgl()
nexus_google_test(
    nexus_lvgl_test
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/lvgl_test.cpp"
    LIBRARIES Nexus::LVGLProfile
)
