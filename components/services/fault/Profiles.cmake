# The M4F implementation is selected explicitly and never enters Native builds.
# Author: Nexus Team
function(nexus_add_cm_backtrace)
    if(TARGET nexus_cm_backtrace)
        return()
    endif()
    get_target_property(cmb_cpu Nexus::Config NEXUS_CPU_ARCH)
    if(NOT cmb_cpu STREQUAL "cortex-m4")
        message(
            FATAL_ERROR
            "CmBacktrace profile requires the selected Cortex-M4 image"
        )
    endif()
    set(cmb_leaf "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    get_filename_component(cmb_root "${cmb_leaf}/../../.." ABSOLUTE)
    include("${cmb_root}/cmake/dependencies/ComponentSource.cmake")
    nexus_component_source(cm_backtrace cmb_source)
    add_library(
        nexus_cm_backtrace_upstream
        STATIC
        EXCLUDE_FROM_ALL
        "${cmb_source}/cm_backtrace/cm_backtrace.c"
    )
    target_include_directories(
        nexus_cm_backtrace_upstream
        PRIVATE "${cmb_leaf}/profile" "${cmb_source}/cm_backtrace"
    )
    target_compile_definitions(
        nexus_cm_backtrace_upstream
        PRIVATE CMB_USER_CFG=1
    )
    target_link_libraries(
        nexus_cm_backtrace_upstream
        PRIVATE nexus_build_options
    )
    target_compile_options(
        nexus_cm_backtrace_upstream
        PRIVATE -mgeneral-regs-only
    )
    add_library(
        nexus_cm_backtrace
        STATIC
        EXCLUDE_FROM_ALL
        "${cmb_leaf}/src/cm_backtrace_adapter.c"
    )
    target_include_directories(nexus_cm_backtrace PRIVATE "${cmb_leaf}/profile")
    target_include_directories(
        nexus_cm_backtrace
        SYSTEM
        PRIVATE "${cmb_source}/cm_backtrace"
    )
    target_compile_definitions(nexus_cm_backtrace PRIVATE CMB_USER_CFG=1)
    target_compile_options(nexus_cm_backtrace PRIVATE -mgeneral-regs-only)
    target_link_libraries(
        nexus_cm_backtrace
        PUBLIC nexus_fault_record
        PRIVATE
            nexus_cm_backtrace_upstream
            nexus_arch
            nexus_owned_warnings
            nexus_build_options
    )
    add_library(Nexus::CmBacktrace ALIAS nexus_cm_backtrace)
    nexus_apply_target_policy(nexus_cm_backtrace_upstream)
    nexus_apply_target_policy(nexus_cm_backtrace)
endfunction()

function(nexus_add_fault_m4_entry)
    if(TARGET nexus_fault_m4_entry)
        return()
    endif()
    get_target_property(fault_cpu Nexus::Config NEXUS_CPU_ARCH)
    if(NOT fault_cpu STREQUAL "cortex-m4")
        message(
            FATAL_ERROR
            "Fault exception entry requires the selected Cortex-M4 image"
        )
    endif()
    set(fault_leaf "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    add_library(
        nexus_fault_m4_entry
        OBJECT
        EXCLUDE_FROM_ALL
        "${fault_leaf}/src/fault_m4_entry.S"
        "${fault_leaf}/src/fault_m4_dispatch.c"
    )
    target_link_libraries(
        nexus_fault_m4_entry
        PUBLIC nexus_fault_record
        PRIVATE nexus_arch nexus_owned_warnings nexus_build_options
    )
    target_compile_options(
        nexus_fault_m4_entry
        PRIVATE "$<$<COMPILE_LANGUAGE:C>:-mgeneral-regs-only>"
    )
    add_library(Nexus::FaultM4Entry ALIAS nexus_fault_m4_entry)
    nexus_apply_target_policy(nexus_fault_m4_entry)
endfunction()
