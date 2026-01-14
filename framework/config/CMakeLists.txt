##############################################################################
# Config Manager Framework Library
##############################################################################

# Config interface library (header-only)
add_library(config_interface INTERFACE)

target_include_directories(config_interface
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Config implementation library
add_library(config_framework STATIC)

target_sources(config_framework
    PRIVATE
        src/config.c
        src/config_store.c
        src/config_types.c
        src/config_default.c
        src/config_namespace.c
        src/config_callback.c
)

target_link_libraries(config_framework
    PUBLIC
        config_interface
    PRIVATE
        osal_interface
)

message(STATUS "Config Manager Framework: Enabled")
