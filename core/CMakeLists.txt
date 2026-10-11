# Small common rules; no registry, heap, kernel or provider state.
add_library(nexus_core STATIC src/time.c src/request.c)
target_include_directories(nexus_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_core PUBLIC nexus_abi_options
    PRIVATE nexus_build_options nexus_owned_warnings)
add_library(Nexus::Core ALIAS nexus_core)
