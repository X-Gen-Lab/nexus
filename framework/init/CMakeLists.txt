add_library(init_interface INTERFACE)
target_include_directories(init_interface INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(init_interface INTERFACE hal_interface)
add_library(init_framework STATIC src/nx_init.c src/nx_firmware_info.c)
target_include_directories(init_framework PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(init_framework PUBLIC init_interface PRIVATE osal)
target_compile_definitions(init_framework PRIVATE
    $<$<CONFIG:Debug>:INIT_DEBUG_ENABLED>
    $<$<BOOL:${NEXUS_BUILD_TESTS}>:NX_INIT_TEST_MODE>)
if(NEXUS_PLATFORM STREQUAL "native")
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_link_options(init_interface INTERFACE "LINKER:-T,${NEXUS_SOURCE_DIR}/cmake/linker/nx_native_init.ld")
        set_property(TARGET init_framework APPEND PROPERTY INTERFACE_LINK_DEPENDS "${NEXUS_SOURCE_DIR}/cmake/linker/nx_native_init.ld")
    elseif(NOT MSVC)
        # These host boundary stubs do not establish initializer support.
        target_sources(init_framework PRIVATE "${NEXUS_SOURCE_DIR}/platforms/native/src/platform/nx_init_sections_stub.c")
    endif()
endif()
