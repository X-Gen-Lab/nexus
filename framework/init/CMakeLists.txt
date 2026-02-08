##############################################################################
# Init Framework Library
##############################################################################

#-----------------------------------------------------------------------------
# Init Interface Library (Header-Only)
#-----------------------------------------------------------------------------

add_library(init_interface INTERFACE)

target_include_directories(init_interface
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

#-----------------------------------------------------------------------------
# Init Implementation Library
#-----------------------------------------------------------------------------

# Init framework sources
set(INIT_SOURCES
    src/nx_init.c
    src/nx_startup.c
    src/nx_firmware_info.c
)

# Create init framework static library
add_library(init_framework STATIC ${INIT_SOURCES})

# Include directories
target_include_directories(init_framework
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(init_framework
    PUBLIC
        init_interface
    PRIVATE
        hal_interface
        osal_interface
)

# Compile definitions
target_compile_definitions(init_framework
    PRIVATE
        $<$<CONFIG:Debug>:INIT_DEBUG_ENABLED>
        $<$<BOOL:${NEXUS_BUILD_TESTS}>:NX_STARTUP_TEST_MODE>
)

# MSVC parallel compilation fix
if(MSVC)
    target_compile_options(init_framework PRIVATE /FS)
endif()

message(STATUS "Init Framework: Configured")

#-----------------------------------------------------------------------------
# End of Init Framework Library
#-----------------------------------------------------------------------------
