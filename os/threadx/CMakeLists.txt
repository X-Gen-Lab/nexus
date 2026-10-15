# Optional actual ThreadX backend; no target is created unless selected.
include("${CMAKE_CURRENT_LIST_DIR}/Kernel.cmake")
if(NEXUS_THREADX_PORT STREQUAL "linux/gnu")
    set(threadx_port linux)
elseif(NEXUS_THREADX_PORT STREQUAL "cortex_m4/gnu" AND
        NEXUS_CPU_ARCH STREQUAL "cortex-m4")
    set(threadx_port cortex_m4)
else()
    message(FATAL_ERROR "Unsupported reviewed ThreadX CPU port")
endif()
nexus_add_threadx_kernel(nx_threadx_kernel "${threadx_port}"
    "${NEXUS_OS_TICK_HZ}" "${NEXUS_THREADX_BASEPRI}")
add_library(nexus_os_threadx STATIC threadx.c)
target_include_directories(nexus_os_threadx PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_os_threadx PUBLIC nexus_os_wait nx_threadx_kernel
    PRIVATE nexus_arch nexus_owned_warnings nexus_build_options)
if(threadx_port STREQUAL "cortex_m4")
    # IRQ entries must precede archives containing weak startup handlers.
    add_library(nx_threadx_bootstrap OBJECT m4.c m4_vectors.S)
    target_link_libraries(nx_threadx_bootstrap PRIVATE nx_threadx_kernel
        nexus_os_wait nexus_arch nexus_owned_warnings nexus_build_options)
    target_include_directories(nx_threadx_bootstrap PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_sources(nexus_os_threadx INTERFACE
        $<TARGET_OBJECTS:nx_threadx_bootstrap>)
endif()
