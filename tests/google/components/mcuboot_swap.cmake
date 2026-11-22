# Actual signed boot, trial/confirm/revert and scratch reset recovery.
# Author: Nexus Team
include("${NEXUS_SOURCE_DIR}/components/protocols/mcuboot/CMakeLists.txt")
nexus_add_mcuboot_swap_boot()
nexus_google_test(nexus_mcuboot_swap_test
    SOURCES "${nexus_google_root}/components/mcuboot_swap_test.cpp"
        "${nexus_google_root}/components/mcuboot_boot_call.c"
        "${NEXUS_SOURCE_DIR}/cmake/package/verification/mcuboot_boot/public_key.c"
    LIBRARIES Nexus::MCUBootSwapBootStorage Nexus::MCUBootSwapControl
        Nexus::MCUBootSwapPort
)
target_compile_definitions(
    nexus_mcuboot_swap_test
    PRIVATE
        NX_MCUBOOT_VALID_IMAGES="${NEXUS_SOURCE_DIR}/tests/contracts/mcuboot_valid_images"
)
