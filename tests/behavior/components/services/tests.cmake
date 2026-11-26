# Explicit services behavior and private model dependencies.
# Author: Nexus Team
include("${nexus_google_root}/components/services/common_diagnostics.cmake")

include(
    "${nexus_google_root}/components/services/common_diagnostics_vendor.cmake"
)

include("${nexus_google_root}/components/services/common_storage.cmake")

nexus_google_test(nexus_settings_test SOURCES "${nexus_google_root}/components/services/settings_test.cpp"
    LIBRARIES Nexus::Settings
)

include("${nexus_google_root}/components/services/mapped_storage.cmake")
