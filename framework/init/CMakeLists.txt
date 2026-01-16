##############################################################################
# Init Framework Library
##############################################################################

# Init interface library (header-only)
add_library(init_interface INTERFACE)

target_include_directories(init_interface
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Init implementation library
add_library(init_framework STATIC)

target_sources(init_framework
    PRIVATE
        src/nx_init.c
        src/nx_startup.c
        src/nx_firmware_info.c
)

target_link_libraries(init_framework
    PUBLIC
        init_interface
    PRIVATE
        hal_interface
        osal_interface
)

# Enable test mode when building tests
if(NEXUS_BUILD_TESTS)
    target_compile_definitions(init_framework PRIVATE NX_STARTUP_TEST_MODE)
endif()

message(STATUS "Init Framework: Enabled")
