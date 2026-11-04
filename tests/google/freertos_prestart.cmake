# Actual pinned POSIX kernel; no scheduler or adapter implementation mocks.
nexus_google_test(nexus_freertos_prestart_test
    SOURCES freertos_prestart_test.cpp
    LIBRARIES contract_os_freertos_posix)

# The child starts a real scheduler; deliberate task return cannot hang the
# parent. This proves port-to-hook wiring, not MCU instruction behavior.
nexus_google_test(nexus_freertos_permanent_return_test
    SOURCES freertos_permanent_return_test.cpp
    LIBRARIES contract_os_freertos_posix)
