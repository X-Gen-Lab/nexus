# Reference host model only; firmware source remains C11.
# Author: Nexus Team
add_library(nexus_soc_external_lifecycle_model STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/lifecycle_adapter.c"
    "${NEXUS_SOURCE_DIR}/tests/support/native/drivers/gpio.c"
    "${NEXUS_SOURCE_DIR}/tests/support/native/drivers/time.c")
target_include_directories(nexus_soc_external_lifecycle_model
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
        "${NEXUS_SOURCE_DIR}/tests/support/native/drivers"
        "${NEXUS_SOURCE_DIR}/io/private")
target_link_libraries(nexus_soc_external_lifecycle_model PUBLIC Nexus::IO Nexus::Arch
    PRIVATE nexus_build_options nexus_owned_warnings)
set(NEXUS_SYSTEM_PRIVATE_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}"
    "${NEXUS_SOURCE_DIR}/tests/support/native/drivers" PARENT_SCOPE)
