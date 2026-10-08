add_library(nexus_update STATIC src/update.c)
target_include_directories(nexus_update PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_update PUBLIC nexus_storage nexus_security)
