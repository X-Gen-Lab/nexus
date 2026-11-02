# Explicit software-only provider integration; no new silicon claim.
# Author: Nexus Team
add_library(nexus_soc_sdk_services_model STATIC
    "${NEXUS_SOURCE_DIR}/soc/native/drivers/gpio.c"
    "${NEXUS_SOURCE_DIR}/soc/native/drivers/time.c")
target_include_directories(nexus_soc_sdk_services_model PRIVATE
    "${NEXUS_SOURCE_DIR}/io/private")
target_link_libraries(nexus_soc_sdk_services_model PUBLIC Nexus::IO Nexus::Arch
    PRIVATE nexus_build_options nexus_owned_warnings)
set(NEXUS_SYSTEM_PRIVATE_INCLUDES
    "${NEXUS_SOURCE_DIR}/soc/native/drivers" PARENT_SCOPE)
