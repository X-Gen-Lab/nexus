# Selected drive profile executes the real locked CANopenNode wire path.
# Author: Nexus Team
nexus_add_cia402()
nexus_google_test(nexus_cia402_test
    SOURCES "${nexus_google_root}/components/protocols/cia402_test.cpp"
        "${nexus_google_root}/components/protocols/cia402_position_test.cpp"
    LIBRARIES Nexus::CiA402
)
