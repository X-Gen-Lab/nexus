# Configuration core consumes only its neutral backend and crypto contracts.
add_library(config_interface INTERFACE)
target_include_directories(config_interface INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(config_interface INTERFACE nexus_build_options)
add_library(config_core STATIC
    src/config.c src/config_store.c src/config_types.c src/config_default.c
    src/config_namespace.c src/config_callback.c src/config_query.c
    src/config_backend.c src/config_export.c src/config_import.c src/config_crypto.c)
target_include_directories(config_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(config_core PUBLIC config_interface nexus_security)
target_compile_definitions(config_core PRIVATE $<$<CONFIG:Debug>:CONFIG_DEBUG_ENABLED>)
add_library(Nexus::ConfigCore ALIAS config_core)

add_library(config_ram_backend STATIC src/config_ram_backend.c)
target_link_libraries(config_ram_backend PUBLIC config_core)
add_library(Nexus::ConfigRAM ALIAS config_ram_backend)
if(TARGET nexus_storage)
    add_library(config_flash_backend STATIC src/config_flash_backend.c)
    target_link_libraries(config_flash_backend PUBLIC config_core nexus_storage)
    add_library(Nexus::ConfigFlash ALIAS config_flash_backend)
endif()

# Convenience composition is explicit. Test fakes are registered in tests only.
add_library(config_framework INTERFACE)
target_link_libraries(config_framework INTERFACE config_core config_ram_backend)
if(TARGET config_flash_backend)
    target_link_libraries(config_framework INTERFACE config_flash_backend)
endif()
