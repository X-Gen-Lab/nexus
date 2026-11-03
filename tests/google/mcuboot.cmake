# Optional MCUboot contracts use the maintained Google registration and core graph.
# Author: Nexus Team
include("${CMAKE_CURRENT_LIST_DIR}/../../components/mcuboot/CMakeLists.txt")
nexus_add_mcuboot_boot()
set(mcuboot_contracts "${CMAKE_CURRENT_LIST_DIR}")
set(mcuboot_component "${mcuboot_contracts}/../../components/mcuboot")

nexus_google_test(nexus_mcuboot_flash_test
    SOURCES "${mcuboot_contracts}/flash_map_backend_test.cpp"
    LIBRARIES Nexus::MCUBootPort)
nexus_google_test(nexus_mcuboot_control_test
    SOURCES "${mcuboot_contracts}/mcuboot_control_test.cpp"
        "${mcuboot_contracts}/control_argument.c"
    LIBRARIES Nexus::MCUBootControl)
nexus_google_test(nexus_mcuboot_public_test
    SOURCES "${mcuboot_contracts}/mcuboot_public_test.cpp"
        "${mcuboot_contracts}/mcuboot_test_ports.cpp"
    LIBRARIES Nexus::MCUBootControl Nexus::MCUBootPort)
nexus_google_test(nexus_mcuboot_boot_test
    SOURCES "${mcuboot_contracts}/mcuboot_boot_test.cpp"
        "${mcuboot_contracts}/mcuboot_test_ports.cpp"
        "${mcuboot_contracts}/mcuboot_boot_call.c"
        "${mcuboot_contracts}/../contracts/mcuboot_link/public_key.c"
    LIBRARIES Nexus::MCUBootBootStorage Nexus::MCUBootControl Nexus::MCUBootPort)
target_compile_definitions(nexus_mcuboot_boot_test PRIVATE
    NX_MCUBOOT_VALID_IMAGES="${mcuboot_contracts}/../contracts/mcuboot_valid_images")
foreach(target nexus_mcuboot_flash_test nexus_mcuboot_control_test
               nexus_mcuboot_public_test nexus_mcuboot_boot_test)
    nexus_apply_target_policy(${target})
endforeach()
