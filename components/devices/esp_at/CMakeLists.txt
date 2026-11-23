# Selected module protocol reuses the existing AT parser and UART owner.
# Author: Nexus Team
add_library(nexus_esp_at STATIC EXCLUDE_FROM_ALL src/esp_at.c)
nexus_apply_target_policy(nexus_esp_at)
target_include_directories(
    nexus_esp_at
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_esp_at
    PUBLIC Nexus::ATClient
    PRIVATE nexus_build_options nexus_owned_warnings
)
add_library(Nexus::ESPAT ALIAS nexus_esp_at)
