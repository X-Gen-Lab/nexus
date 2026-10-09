add_library(nexus_industrial STATIC src/industrial_supervisor.c)
target_include_directories(nexus_industrial PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_industrial PUBLIC nexus_build_options)
