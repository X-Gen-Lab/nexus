add_library(nexus_owner_observer STATIC src/owner_observer.c)
target_include_directories(nexus_owner_observer PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_owner_observer PUBLIC Nexus::BusOwner Nexus::Core
    PRIVATE nexus_build_options nexus_owned_warnings)
add_library(Nexus::OwnerObserver ALIAS nexus_owner_observer)
