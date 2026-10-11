# This image links the real Native Arch library, with no hardware replacement.
nexus_google_test(nexus_arch_native_mechanism_test
    SOURCES arch_native_mechanism_test.cpp LIBRARIES Nexus::Arch)
