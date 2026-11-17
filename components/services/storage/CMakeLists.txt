# Explicit blob store, physical Flash adapter and Native file model.
# Author: Nexus Team
add_library(nexus_storage STATIC src/storage.c)
target_include_directories(
    nexus_storage
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_storage
    PUBLIC nexus_abi_options
    PRIVATE nexus_owned_warnings nexus_build_options
)
add_library(nexus_flash_storage STATIC src/flash_storage.c)
target_link_libraries(
    nexus_flash_storage
    PUBLIC nexus_storage Nexus::IOFlash
    PRIVATE nexus_owned_warnings nexus_build_options
)
if(NEXUS_SOC_FAMILY STREQUAL "native")
    add_library(nexus_file_flash STATIC src/file_flash.c)
    target_link_libraries(
        nexus_file_flash
        PUBLIC nexus_storage
        PRIVATE nexus_owned_warnings nexus_build_options
    )
endif()
add_library(Nexus::Storage ALIAS nexus_storage)
add_library(Nexus::FlashStorage ALIAS nexus_flash_storage)
if(TARGET nexus_file_flash)
    add_library(Nexus::FileFlash ALIAS nexus_file_flash)
endif()
