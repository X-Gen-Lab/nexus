# Optional MCUboot contracts use the maintained Google registration and core graph.
# Author: Nexus Team
include("${NEXUS_SOURCE_DIR}/components/protocols/mcuboot/CMakeLists.txt")
nexus_add_mcuboot_boot()
set(mcuboot_contracts "${nexus_google_root}/components/protocols")
set(mcuboot_component "${NEXUS_SOURCE_DIR}/components/protocols/mcuboot")

nexus_google_test(nexus_mcuboot_flash_test
    SOURCES "${nexus_google_root}/io/flash_map_backend_test.cpp"
    LIBRARIES Nexus::MCUBootPort
)
nexus_google_test(nexus_mcuboot_control_test
    SOURCES "${mcuboot_contracts}/mcuboot_control_test.cpp"
        "${NEXUS_SOURCE_DIR}/tests/integration/c_api/mcuboot/control_argument.c"
    LIBRARIES Nexus::MCUBootControl
)
nexus_google_test(nexus_mcuboot_public_test
    SOURCES "${mcuboot_contracts}/mcuboot_public_test.cpp"
        "${mcuboot_contracts}/mcuboot_test_ports.cpp"
    LIBRARIES Nexus::MCUBootControl Nexus::MCUBootPort
)
nexus_google_test(nexus_mcuboot_boot_test
    SOURCES "${mcuboot_contracts}/mcuboot_boot_test.cpp"
        "${mcuboot_contracts}/mcuboot_test_ports.cpp"
        "${mcuboot_contracts}/mcuboot_boot_call.c"
        "${NEXUS_SOURCE_DIR}/cmake/package/verification/mcuboot_boot/public_key.c"
    LIBRARIES Nexus::MCUBootBootStorage Nexus::MCUBootControl Nexus::MCUBootPort
)
target_compile_definitions(
    nexus_mcuboot_boot_test
    PRIVATE
        NX_MCUBOOT_VALID_IMAGES="${NEXUS_SOURCE_DIR}/tests/integration/protocols/mcuboot_valid_images"
)
foreach(
    target
    nexus_mcuboot_flash_test
    nexus_mcuboot_control_test
    nexus_mcuboot_public_test
    nexus_mcuboot_boot_test
)
    nexus_apply_target_policy(${target})
endforeach()
