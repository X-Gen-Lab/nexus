# Actual pinned POSIX kernel; no scheduler or adapter implementation mocks.
nexus_google_test(nexus_freertos_prestart_test
    SOURCES freertos_prestart_test.cpp
    LIBRARIES contract_os_freertos_posix)
