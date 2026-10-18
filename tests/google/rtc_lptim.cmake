# Native-only source contracts; no physical RTC/LPTIM claim.
nexus_google_test(nexus_rtc_lptim_test SOURCES rtc_lptim_test.cpp
    LIBRARIES Nexus::NativeModel
    INCLUDES "${NEXUS_SOURCE_DIR}/io/native")
if(UNIX AND NOT APPLE)
    nexus_google_test(nexus_rtc_lptim_final_access_test
        SOURCES rtc_lptim_final_access_test.cpp
        LIBRARIES Nexus::NativeModel
        INCLUDES "${NEXUS_SOURCE_DIR}/io/native")
endif()

nexus_google_test(nexus_rtc_lptim_start_test SOURCES rtc_lptim_start_test.cpp
    LIBRARIES Nexus::NativeModel
    INCLUDES "${NEXUS_SOURCE_DIR}/io/native")
