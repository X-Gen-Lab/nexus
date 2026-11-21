# Preserve the actual parser; only its port and fixed read-only table are Nexus.
# Author: Nexus Team
function(nexus_add_shell)
    if(TARGET nexus_shell)
        return()
    endif()
    set(shell_leaf "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    get_filename_component(shell_root "${shell_leaf}/../../.." ABSOLUTE)
    include("${shell_root}/cmake/dependencies/ComponentSource.cmake")
    nexus_component_source(letter_shell shell_source)
    add_library(
        nexus_letter_shell
        STATIC
        EXCLUDE_FROM_ALL
        "${shell_source}/src/shell.c"
        "${shell_source}/src/shell_ext.c"
    )
    target_include_directories(
        nexus_letter_shell
        PRIVATE "${shell_leaf}/profile" "${shell_source}/src"
    )
    target_compile_definitions(
        nexus_letter_shell
        PRIVATE "SHELL_CFG_USER=\"nexus_shell_cfg.h\""
    )
    target_link_libraries(nexus_letter_shell PRIVATE nexus_build_options)
    add_library(nexus_shell STATIC EXCLUDE_FROM_ALL "${shell_leaf}/src/shell.c")
    target_include_directories(
        nexus_shell
        PUBLIC "${shell_leaf}/include"
        PRIVATE
            "${shell_leaf}/profile"
            "${shell_root}/components/services/log/private"
    )
    target_include_directories(nexus_shell SYSTEM PRIVATE "${shell_source}/src")
    target_compile_definitions(
        nexus_shell
        PRIVATE "SHELL_CFG_USER=\"nexus_shell_cfg.h\""
    )
    target_link_libraries(
        nexus_shell
        PUBLIC nexus_fault_record nexus_health
        PRIVATE
            nexus_letter_shell
            nexus_log_formatter
            nexus_arch
            nexus_owned_warnings
            nexus_build_options
    )
    add_library(Nexus::Shell ALIAS nexus_shell)
    add_library(nexus_shell_storage INTERFACE)
    target_include_directories(
        nexus_shell_storage
        INTERFACE "${shell_leaf}/profile"
    )
    target_include_directories(
        nexus_shell_storage
        SYSTEM
        INTERFACE "${shell_source}/src"
    )
    target_compile_definitions(
        nexus_shell_storage
        INTERFACE "SHELL_CFG_USER=\"nexus_shell_cfg.h\""
    )
    target_link_libraries(nexus_shell_storage INTERFACE nexus_shell)
    add_library(Nexus::ShellStorage ALIAS nexus_shell_storage)
    nexus_apply_target_policy(nexus_letter_shell)
    nexus_apply_target_policy(nexus_shell)
endfunction()
