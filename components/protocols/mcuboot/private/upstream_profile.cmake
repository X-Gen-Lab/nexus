# Official source graph shared by independent host and ARM profiles.
# Author: Nexus Team
function(nexus_mcuboot_upstream_target source)
    if(NOT TARGET bootutil)
        add_subdirectory(
            "${source}/boot/bootutil"
            "${CMAKE_CURRENT_BINARY_DIR}/upstream-bootutil"
            EXCLUDE_FROM_ALL
        )
        set_property(
            TARGET bootutil
            PROPERTY NEXUS_MCUBOOT_UPSTREAM_ROOT "${source}"
        )
    endif()
    get_target_property(owned bootutil NEXUS_MCUBOOT_UPSTREAM_ROOT)
    if(NOT owned STREQUAL source)
        message(FATAL_ERROR "MCUboot source graph belongs to another owner")
    endif()
endfunction()

function(nexus_mcuboot_crypto_target target source port profile)
    add_library(
        ${target}
        STATIC
        EXCLUDE_FROM_ALL
        "${source}/ext/tinycrypt/lib/source/ecc.c"
        "${source}/ext/tinycrypt/lib/source/ecc_dsa.c"
        "${source}/ext/tinycrypt/lib/source/sha256.c"
        "${source}/ext/tinycrypt/lib/source/utils.c"
        "${source}/ext/mbedtls-asn1/src/asn1parse.c"
        "${source}/ext/mbedtls-asn1/src/platform_util.c"
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Native tooling can retain instrumented, otherwise unused RNG globals.
        # Use the official POSIX port; MCU verification never adds this source.
        target_sources(
            ${target}
            PRIVATE "${source}/ext/tinycrypt/lib/source/ecc_platform_specific.c"
        )
    endif()
    nexus_apply_target_policy(${target})
    target_include_directories(
        ${target}
        PRIVATE
            "${port}/port"
            "${profile}"
            "${port}/profile"
            "${source}/ext/tinycrypt/lib/include"
            "${source}/ext/mbedtls-asn1/include"
            "${source}/boot/zephyr/include"
    )
    target_compile_definitions(
        ${target}
        PRIVATE MBEDTLS_CONFIG_FILE="config-asn1.h"
    )
    target_link_libraries(${target} PRIVATE nexus_build_options)
endfunction()

function(
    nexus_mcuboot_core_target
    name
    source
    port
    profile
    flash
    crypto
    output
)
    nexus_mcuboot_upstream_target("${source}")
    get_target_property(selected bootutil NEXUS_MCUBOOT_CORE_PROFILE)
    if(NOT selected)
        # The first actual profile owns the authoritative upstream target.
        set(core bootutil)
    else()
        # A second profile compiles that exact source list independently.
        get_target_property(upstream_sources bootutil SOURCES)
        get_target_property(upstream_root bootutil SOURCE_DIR)
        set(selected_sources)
        foreach(file IN LISTS upstream_sources)
            get_filename_component(
                path
                "${file}"
                ABSOLUTE
                BASE_DIR "${upstream_root}"
            )
            list(APPEND selected_sources "${path}")
        endforeach()
        set(core "${name}")
        add_library(${core} STATIC EXCLUDE_FROM_ALL ${selected_sources})
    endif()
    set_property(
        TARGET ${core}
        PROPERTY NEXUS_MCUBOOT_CORE_PROFILE "${profile}"
    )
    nexus_apply_target_policy(${core})
    target_include_directories(
        ${core}
        PRIVATE
            "${port}/port"
            "${profile}"
            "${port}/profile"
            "${source}/boot/bootutil/src"
            "${source}/ext/tinycrypt/lib/include"
            "${source}/ext/mbedtls-asn1/include"
            "${source}/boot/zephyr/include"
    )
    target_include_directories(
        ${core}
        SYSTEM
        PUBLIC "${source}/boot/bootutil/include"
    )
    target_compile_definitions(
        ${core}
        PRIVATE MBEDTLS_CONFIG_FILE="config-asn1.h"
    )
    target_link_libraries(
        ${core}
        PRIVATE "${flash}" "${crypto}" nexus_build_options
    )
    set(${output} "${core}" PARENT_SCOPE)
endfunction()

function(nexus_reference_mcuboot_core source port flash)
    nexus_mcuboot_crypto_target(
        mcuboot_crypto "${source}" "${port}" "${port}/profile"
    )
    nexus_mcuboot_core_target(
        nexus_mcuboot_overwrite_bootutil "${source}" "${port}" "${port}/profile"
        "${flash}" mcuboot_crypto core
    )
    add_library(nexus_mcuboot_boot_storage INTERFACE)
    set_property(
        TARGET nexus_mcuboot_boot_storage
        PROPERTY INTERFACE_NEXUS_CRYPTO_ABI mcuboot-tinycrypt-asn1
    )
    set_property(
        TARGET nexus_mcuboot_boot_storage
        APPEND
        PROPERTY COMPATIBLE_INTERFACE_STRING NEXUS_CRYPTO_ABI
    )
    target_link_libraries(nexus_mcuboot_boot_storage INTERFACE "${core}")
    # The fixed verifier has no allocator: discard unselected supplier branches.
    target_link_options(
        nexus_mcuboot_boot_storage
        INTERFACE "LINKER:--gc-sections"
    )
    target_include_directories(
        nexus_mcuboot_boot_storage
        INTERFACE "${port}/profile" "${port}/port"
    )
    add_library(Nexus::MCUBootBootStorage ALIAS nexus_mcuboot_boot_storage)
    nexus_mcuboot_profile(nexus_mcuboot_boot_storage overwrite-only)
endfunction()
