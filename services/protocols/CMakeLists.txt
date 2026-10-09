add_library(nexus_modbus_rtu STATIC src/modbus_rtu.c)
target_include_directories(nexus_modbus_rtu PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_modbus_rtu PUBLIC nexus_build_options)
