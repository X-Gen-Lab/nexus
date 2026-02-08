##############################################################################
# Config Manager Framework Library
##############################################################################

#-----------------------------------------------------------------------------
# Config Interface Library (Header-Only)
#-----------------------------------------------------------------------------

add_library(config_interface INTERFACE)

target_include_directories(config_interface
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

#-----------------------------------------------------------------------------
# Config Implementation Library
#-----------------------------------------------------------------------------

# Config framework sources
set(CONFIG_SOURCES
    src/config.c
    src/config_store.c
    src/config_types.c
    src/config_default.c
    src/config_namespace.c
    src/config_callback.c
    src/config_query.c
    src/config_backend.c
    src/config_ram_backend.c
    src/config_flash_backend.c
    src/config_mock_backend.c
    src/config_export.c
    src/config_import.c
    src/config_crypto.c
)

# Create config framework static library
add_library(config_framework STATIC ${CONFIG_SOURCES})

# Include directories
target_include_directories(config_framework
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(config_framework
    PUBLIC
        config_interface
    PRIVATE
        osal_interface
)

# Compile definitions
target_compile_definitions(config_framework
    PRIVATE
        $<$<CONFIG:Debug>:CONFIG_DEBUG_ENABLED>
)

# MSVC parallel compilation fix
if(MSVC)
    target_compile_options(config_framework PRIVATE /FS)
endif()

message(STATUS "Config Manager Framework: Configured")

#-----------------------------------------------------------------------------
# End of Config Manager Framework Library
#-----------------------------------------------------------------------------
