# Exercise the real Native Arch implementation, without replacing its symbols.
find_package(Threads REQUIRED)
nexus_google_test(nexus_arch_native_test SOURCES arch_native_test.cpp
    LIBRARIES Nexus::Arch Threads::Threads)
