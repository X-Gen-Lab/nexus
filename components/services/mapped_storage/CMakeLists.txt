# Optional allocation-free mapped-loan ownership and finite storage requests.
add_library(nexus_mapped_storage STATIC
    src/mapped_storage.c src/mapped_storage_owner.c)
target_include_directories(nexus_mapped_storage PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_mapped_storage PUBLIC Nexus::Core
    PRIVATE nexus_build_options nexus_owned_warnings)
add_library(Nexus::MappedStorage ALIAS nexus_mapped_storage)

# Coherent host NOR model: never selected or linked by a firmware target.
if(NEXUS_SOC_FAMILY STREQUAL "native")
    add_library(nexus_native_mapped_storage STATIC src/native_mapped_storage.c)
    target_link_libraries(nexus_native_mapped_storage PUBLIC Nexus::MappedStorage
        PRIVATE nexus_build_options nexus_owned_warnings)
    add_library(Nexus::NativeMappedStorage ALIAS nexus_native_mapped_storage)
endif()
