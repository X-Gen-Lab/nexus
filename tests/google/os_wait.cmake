# Actual Native/Baremetal waits; wrappers observe only the POSIX boundary.
find_package(Threads REQUIRED)
nexus_google_test(nexus_os_wait_test SOURCES os_wait_test.cpp
    LIBRARIES Nexus::OSWait Nexus::OSNative Nexus::OSBaremetal Threads::Threads)
target_link_options(nexus_os_wait_test PRIVATE
    "-Wl,--wrap=pthread_cond_wait" "-Wl,--wrap=pthread_cond_timedwait"
    "-Wl,--wrap=clock_gettime")
