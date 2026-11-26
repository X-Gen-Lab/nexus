# Real storage consumers use only external Flash and Block software media.
# Author: Nexus Team
nexus_google_test(nexus_common_fal_test
    SOURCES "${nexus_google_root}/components/services/common_fal_test.cpp"
    LIBRARIES Nexus::FAL
)
nexus_add_flashdb()
nexus_google_test(nexus_common_flashdb_test
    SOURCES "${nexus_google_root}/components/services/common_flashdb_test.cpp"
    LIBRARIES Nexus::FlashDB
)
nexus_add_littlefs()
nexus_google_test(nexus_common_littlefs_test
    SOURCES "${nexus_google_root}/components/services/common_littlefs_test.cpp"
    LIBRARIES Nexus::LittleFS
)
nexus_add_fatfs()
nexus_google_test(nexus_common_fatfs_test
    SOURCES "${nexus_google_root}/components/services/common_fatfs_test.cpp"
    LIBRARIES Nexus::FatFS
)
nexus_add_sfud()
nexus_google_test(nexus_common_sfud_test
    SOURCES "${nexus_google_root}/components/devices/common_sfud_test.cpp"
    LIBRARIES Nexus::SFUD Nexus::FlashDB
)
nexus_google_test(nexus_common_record_test
    SOURCES "${nexus_google_root}/components/services/common_record_test.cpp"
    LIBRARIES Nexus::FlashDB Nexus::Algorithms
)
