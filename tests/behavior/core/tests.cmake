# Explicit core behavior targets and private model requirements.
# Author: Nexus Team
nexus_google_test(nexus_realtime_metadata_test SOURCES "${nexus_google_root}/core/realtime_metadata_test.cpp"
    LIBRARIES Nexus::BusOwner Nexus::IO Nexus::Arch
)

nexus_google_test(nexus_polymorphic_io_test SOURCES "${nexus_google_root}/core/polymorphic_mock_test.cpp"
    LIBRARIES Nexus::IO
)

nexus_google_test(nexus_core_lifecycle_test SOURCES "${nexus_google_root}/core/core_lifecycle_test.cpp"
    LIBRARIES Nexus::Core
)

nexus_google_test(nexus_platform_lifecycle_test SOURCES "${nexus_google_root}/core/platform_lifecycle_test.cpp"
    LIBRARIES Nexus::Core
)

include("${nexus_google_root}/core/atomic_tests.cmake")
