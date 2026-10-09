add_library(nexus_storage STATIC src/storage.c)
target_include_directories(nexus_storage PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_storage PUBLIC nexus_build_options)
if(NEXUS_PLATFORM STREQUAL "native" AND UNIX)
    add_library(nexus_file_flash STATIC src/file_flash.c)
    target_link_libraries(nexus_file_flash PUBLIC nexus_storage)
endif()

# Explicit typed HAL adapter; StorageCore remains independent of hardware/OSAL.
add_library(nexus_storage_hal STATIC adapters/hal_flash.c)
target_link_libraries(nexus_storage_hal PUBLIC nexus_storage hal_interface PRIVATE hal_flash nexus_arch)
add_library(Nexus::StorageHAL ALIAS nexus_storage_hal)
