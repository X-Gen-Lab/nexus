add_library(config_interface INTERFACE)
target_include_directories(config_interface INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(config_interface INTERFACE nexus_storage)
add_library(config_framework STATIC
    src/config.c src/config_store.c src/config_types.c src/config_default.c
    src/config_namespace.c src/config_callback.c src/config_query.c
    src/config_backend.c src/config_ram_backend.c src/config_flash_backend.c
    src/config_mock_backend.c src/config_export.c src/config_import.c src/config_crypto.c)
target_include_directories(config_framework PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(config_framework PUBLIC config_interface nexus_security)
target_compile_definitions(config_framework PRIVATE $<$<CONFIG:Debug>:CONFIG_DEBUG_ENABLED>)
