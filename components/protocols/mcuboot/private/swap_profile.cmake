# Independent scratch profile reuses the authoritative upstream target sources.
# Author: Nexus Team
function(nexus_add_mcuboot_swap_control)
    if(NOT ARGC EQUAL 0)
        message(
            FATAL_ERROR
            "nexus_add_mcuboot_swap_control accepts no overrides"
        )
    endif()
    if(TARGET Nexus::MCUBootSwapControl)
        return()
    endif()
    _nexus_mcuboot_admit_source(source)
    get_filename_component(
        component
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.."
        ABSOLUTE
    )
    add_library(
        nexus_mcuboot_swap_flash
        STATIC
        EXCLUDE_FROM_ALL
        "${component}/src/flash_map_backend.c"
    )
    nexus_apply_target_policy(nexus_mcuboot_swap_flash)
    target_compile_definitions(
        nexus_mcuboot_swap_flash
        PRIVATE NEXUS_MCUBOOT_SWAP_SCRATCH=1
    )
    target_include_directories(
        nexus_mcuboot_swap_flash
        PUBLIC "${component}/port"
        PRIVATE "${component}/swap/profile"
    )
    target_link_libraries(
        nexus_mcuboot_swap_flash
        PUBLIC Nexus::IOFlash
        PRIVATE nexus_build_options nexus_owned_warnings
    )
    add_library(nexus_mcuboot_swap_port INTERFACE)
    target_link_libraries(
        nexus_mcuboot_swap_port
        INTERFACE nexus_mcuboot_swap_flash
    )
    nexus_mcuboot_profile(nexus_mcuboot_swap_port scratch)
    add_library(Nexus::MCUBootSwapPort ALIAS nexus_mcuboot_swap_port)
    add_library(
        nexus_mcuboot_swap_public
        STATIC
        EXCLUDE_FROM_ALL
        "${source}/boot/bootutil/src/bootutil_public.c"
    )
    nexus_apply_target_policy(nexus_mcuboot_swap_public)
    target_include_directories(
        nexus_mcuboot_swap_public
        PRIVATE
            "${component}/port"
            "${component}/swap/profile"
            "${component}/profile"
            "${source}/boot/bootutil/src"
    )
    target_include_directories(
        nexus_mcuboot_swap_public
        SYSTEM
        PRIVATE "${source}/boot/bootutil/include"
    )
    target_link_libraries(
        nexus_mcuboot_swap_public
        PRIVATE nexus_mcuboot_swap_flash nexus_build_options
    )
    add_library(
        nexus_mcuboot_swap_control
        STATIC
        EXCLUDE_FROM_ALL
        "${component}/src/mcuboot_swap_control.c"
    )
    nexus_apply_target_policy(nexus_mcuboot_swap_control)
    target_include_directories(
        nexus_mcuboot_swap_control
        PUBLIC "${component}/include"
        PRIVATE
            "${component}/port"
            "${component}/swap/profile"
            "${component}/profile"
    )
    target_include_directories(
        nexus_mcuboot_swap_control
        SYSTEM
        PRIVATE "${source}/boot/bootutil/include"
    )
    target_link_libraries(
        nexus_mcuboot_swap_control
        PUBLIC Nexus::Core
        PRIVATE
            nexus_mcuboot_swap_public
            nexus_mcuboot_swap_flash
            nexus_build_options
            nexus_owned_warnings
    )
    set_property(
        TARGET nexus_mcuboot_swap_control
        PROPERTY NEXUS_MCUBOOT_SOURCE "${source}"
    )
    nexus_mcuboot_profile(nexus_mcuboot_swap_control scratch)
    add_library(Nexus::MCUBootSwapControl ALIAS nexus_mcuboot_swap_control)
endfunction()

function(nexus_add_mcuboot_swap_boot)
    if(NOT ARGC EQUAL 0)
        message(FATAL_ERROR "nexus_add_mcuboot_swap_boot accepts no overrides")
    endif()
    if(TARGET Nexus::MCUBootSwapBootStorage)
        return()
    endif()
    if(
        NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang"
        OR (NOT CMAKE_CROSSCOMPILING AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    )
        message(
            FATAL_ERROR
            "MCUboot scratch profile requires GNU/LLVM MCU or Linux"
        )
    endif()
    nexus_add_mcuboot_swap_control()
    get_target_property(source Nexus::MCUBootSwapControl NEXUS_MCUBOOT_SOURCE)
    get_filename_component(
        component
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.."
        ABSOLUTE
    )
    include("${component}/private/upstream_profile.cmake")
    nexus_mcuboot_crypto_target(
        nexus_mcuboot_swap_crypto "${source}" "${component}" "${component}/swap/profile"
    )
    nexus_mcuboot_core_target(
        nexus_mcuboot_swap_bootutil "${source}" "${component}"
        "${component}/swap/profile" nexus_mcuboot_swap_flash
        nexus_mcuboot_swap_crypto core
    )
    add_library(nexus_mcuboot_swap_boot_storage INTERFACE)
    target_link_libraries(nexus_mcuboot_swap_boot_storage INTERFACE "${core}")
    target_include_directories(
        nexus_mcuboot_swap_boot_storage
        INTERFACE
            "${component}/swap/profile"
            "${component}/profile"
            "${component}/port"
    )
    target_link_options(
        nexus_mcuboot_swap_boot_storage
        INTERFACE "LINKER:--gc-sections"
    )
    set_property(
        TARGET nexus_mcuboot_swap_boot_storage
        PROPERTY INTERFACE_NEXUS_CRYPTO_ABI mcuboot-tinycrypt-asn1
    )
    set_property(
        TARGET nexus_mcuboot_swap_boot_storage
        APPEND
        PROPERTY COMPATIBLE_INTERFACE_STRING NEXUS_CRYPTO_ABI
    )
    nexus_mcuboot_profile(nexus_mcuboot_swap_boot_storage scratch)
    add_library(
        Nexus::MCUBootSwapBootStorage
        ALIAS nexus_mcuboot_swap_boot_storage
    )
endfunction()
