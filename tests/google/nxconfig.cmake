# The host configuration pilot shares its exact implementation and contracts
# with standalone bootstrap. No parser library joins a firmware target.
include("${NEXUS_SOURCE_DIR}/tools/nxconfig/cmake/NxconfigCore.cmake")
# Instrument the C implementation and parsers without inheriting MCU/assembly
# optimization, section or ABI options from nexus_build_options.
foreach(target IN ITEMS nxconfig_core nxconfig_toml nxconfig_json)
    target_link_libraries(${target} PRIVATE nexus_sanitizer_options)
endforeach()
nexus_google_test(nexus_nxconfig_test
    SOURCES "${NEXUS_SOURCE_DIR}/tools/nxconfig/tests/config_test.cpp"
    LIBRARIES nxconfig_core)
nxconfig_contract_settings(nexus_nxconfig_test)
