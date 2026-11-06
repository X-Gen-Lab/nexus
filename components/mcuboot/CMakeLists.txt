# Exact optional control and separate boot-profile targets; no runtime workers.
# Author: Nexus Team
include_guard(GLOBAL)

function(_nexus_mcuboot_admit_source output)
    if(NOT TARGET Nexus::Config OR NOT TARGET Nexus::IO)
        message(FATAL_ERROR "MCUboot requires a configured Nexus assembly")
    endif()
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    set(component "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    get_filename_component(root "${component}/../.." ABSOLUTE)
    get_target_property(assembly_root Nexus::Config NEXUS_SOURCE_DIR)
    if(NOT root STREQUAL assembly_root AND NOT NEXUS_MCUBOOT_DEVELOPMENT)
        message(FATAL_ERROR "MCUboot component and assembly source differ")
    endif()
    if(DEFINED NEXUS_MCUBOOT_SOURCE AND NOT NEXUS_MCUBOOT_SOURCE STREQUAL "")
        if(IS_ABSOLUTE "${NEXUS_MCUBOOT_SOURCE}")
            set(source "${NEXUS_MCUBOOT_SOURCE}")
        else()
            # Preserve the selected spelling; the verifier rejects aliases.
            set(source "${component}/${NEXUS_MCUBOOT_SOURCE}")
        endif()
    else()
        set(source "${root}/ext/mcuboot")
    endif()
    set(scope)
    if(NEXUS_MCUBOOT_DEVELOPMENT)
        list(APPEND scope --development)
    else()
        get_property(sdk GLOBAL PROPERTY NEXUS_SOURCE_SDK_SOURCE)
        if(sdk AND sdk STREQUAL root)
            list(APPEND scope --source-sdk)
        endif()
    endif()
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${root}/tools/maintenance/mcuboot_lock.py"
        --source "${source}" --lock "${root}/dependencies/mcuboot.lock.json"
        ${scope}
        RESULT_VARIABLE verified OUTPUT_VARIABLE proof ERROR_VARIABLE error)
    if(NOT verified EQUAL 0)
        message(FATAL_ERROR "MCUboot source rejected: ${proof}${error}")
    endif()
    get_filename_component(source "${source}" REALPATH)
    string(JSON count LENGTH "${proof}" configure_inputs)
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
        string(JSON input GET "${proof}" configure_inputs ${index})
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input}")
    endforeach()
    foreach(input tools/maintenance/mcuboot_lock.py tools/dev/dependencies.py
                  tools/maintenance/source_catalogue.py
                  tools/configure/metadata.py)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${root}/${input}")
    endforeach()
    set(${output} "${source}" PARENT_SCOPE)
endfunction()

function(nexus_add_mcuboot_control)
    if(NOT ARGC EQUAL 0)
        message(FATAL_ERROR "nexus_add_mcuboot_control accepts no overrides")
    endif()
    if(TARGET Nexus::MCUBootControl)
        return()
    endif()
    _nexus_mcuboot_admit_source(source)
    set(component "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    add_library(nexus_mcuboot_flash STATIC EXCLUDE_FROM_ALL
        "${component}/src/flash_map_backend.c")
    nexus_apply_target_policy(nexus_mcuboot_flash)
    target_include_directories(nexus_mcuboot_flash PUBLIC "${component}/port")
    target_link_libraries(nexus_mcuboot_flash PUBLIC Nexus::IOFlash
        PRIVATE nexus_build_options nexus_owned_warnings)
    add_library(nexus_mcuboot_port INTERFACE)
    target_link_libraries(nexus_mcuboot_port INTERFACE nexus_mcuboot_flash)
    add_library(Nexus::MCUBootPort ALIAS nexus_mcuboot_port)

    add_library(nexus_mcuboot_public STATIC EXCLUDE_FROM_ALL
        "${source}/boot/bootutil/src/bootutil_public.c")
    nexus_apply_target_policy(nexus_mcuboot_public)
    target_include_directories(nexus_mcuboot_public PRIVATE
        "${component}/port" "${component}/profile"
        "${source}/boot/bootutil/src")
    target_include_directories(nexus_mcuboot_public SYSTEM PRIVATE
        "${source}/boot/bootutil/include")
    target_link_libraries(nexus_mcuboot_public PRIVATE nexus_mcuboot_flash
        nexus_build_options)

    add_library(nexus_mcuboot_control STATIC EXCLUDE_FROM_ALL
        "${component}/src/mcuboot_control.c")
    nexus_apply_target_policy(nexus_mcuboot_control)
    target_include_directories(nexus_mcuboot_control PUBLIC "${component}/include")
    target_include_directories(nexus_mcuboot_control PRIVATE
        "${component}/port" "${component}/profile")
    target_include_directories(nexus_mcuboot_control SYSTEM PRIVATE
        "${source}/boot/bootutil/include")
    target_link_libraries(nexus_mcuboot_control PUBLIC Nexus::Core
        PRIVATE nexus_mcuboot_public nexus_mcuboot_flash
        nexus_build_options nexus_owned_warnings)
    set_property(TARGET nexus_mcuboot_control PROPERTY
        NEXUS_MCUBOOT_SOURCE "${source}")
    add_library(Nexus::MCUBootControl ALIAS nexus_mcuboot_control)
endfunction()

function(nexus_add_mcuboot_boot)
    if(NOT ARGC EQUAL 0)
        message(FATAL_ERROR "nexus_add_mcuboot_boot accepts no overrides")
    endif()
    if(TARGET Nexus::MCUBootBootStorage)
        return()
    endif()
    if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" OR
       (NOT CMAKE_CROSSCOMPILING AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux"))
        message(FATAL_ERROR
            "Reviewed MCUboot boot profile requires GNU/LLVM MCU or Linux")
    endif()
    if(TARGET bootutil OR TARGET mcuboot_crypto)
        message(FATAL_ERROR "MCUboot requires its reviewed separate core profile")
    endif()
    nexus_add_mcuboot_control()
    get_target_property(source Nexus::MCUBootControl NEXUS_MCUBOOT_SOURCE)
    set(component "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    include("${component}/private/upstream_profile.cmake")
    nexus_reference_mcuboot_core("${source}" "${component}" nexus_mcuboot_flash)
endfunction()
