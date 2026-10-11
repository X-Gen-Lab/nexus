# Real production optional mechanisms with only external boundaries mocked.
nexus_google_test(nexus_arch_sleep_test SOURCES arch_sleep_test.cpp
    "${NEXUS_SOURCE_DIR}/arch/cortex_m/nx_arch_sleep.c"
    INCLUDES "${NEXUS_SOURCE_DIR}/arch/include"
        "${NEXUS_SOURCE_DIR}/tests/google")
target_compile_definitions(nexus_arch_sleep_test PRIVATE NEXUS_ARCH_SLEEP_MODEL)

nexus_google_test(nexus_os_diagnostic_test SOURCES os_diagnostic_test.cpp
    "${NEXUS_SOURCE_DIR}/os/diagnostic.c"
    INCLUDES "${NEXUS_SOURCE_DIR}/os/include"
        "${NEXUS_SOURCE_DIR}/core/include"
        "${NEXUS_SOURCE_DIR}/arch/include")

nexus_google_test(nexus_os_lowpower_test SOURCES os_lowpower_test.cpp
    "${NEXUS_SOURCE_DIR}/os/freertos/lowpower.c"
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/google/lowpower_profile"
        "${NEXUS_SOURCE_DIR}/os/freertos/include"
        "${NEXUS_SOURCE_DIR}/core/include"
        "${NEXUS_SOURCE_DIR}/arch/include")
target_include_directories(nexus_os_lowpower_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/ext/freertos/include"
    "${NEXUS_SOURCE_DIR}/ext/freertos/portable/ThirdParty/GCC/Posix")

nexus_google_test(nexus_os_diagnostic_native_test
    SOURCES os_diagnostic_native_test.cpp
    LIBRARIES Nexus::OSDiagnostic Nexus::Arch)
