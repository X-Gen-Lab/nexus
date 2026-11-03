# Official fixed-profile source graph shared by host and ARM references.
# Author: Nexus Team
function(nexus_reference_mcuboot_core source port flash)
    add_library(mcuboot_crypto STATIC
        "${source}/ext/tinycrypt/lib/source/ecc.c"
        "${source}/ext/tinycrypt/lib/source/ecc_dsa.c"
        "${source}/ext/tinycrypt/lib/source/sha256.c"
        "${source}/ext/tinycrypt/lib/source/utils.c"
        "${source}/ext/mbedtls-asn1/src/asn1parse.c"
        "${source}/ext/mbedtls-asn1/src/platform_util.c")
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Native tooling can retain instrumented, otherwise unused RNG globals.
        # Use the official POSIX port; MCU verification never adds this source.
        target_sources(mcuboot_crypto PRIVATE
            "${source}/ext/tinycrypt/lib/source/ecc_platform_specific.c")
    endif()
    nexus_apply_target_policy(mcuboot_crypto)
    target_include_directories(mcuboot_crypto PRIVATE "${port}/port" "${port}/profile"
        "${source}/ext/tinycrypt/lib/include"
        "${source}/ext/mbedtls-asn1/include"
        "${source}/boot/zephyr/include")
    target_compile_definitions(mcuboot_crypto PRIVATE
        MBEDTLS_CONFIG_FILE="config-asn1.h")
    target_link_libraries(mcuboot_crypto PRIVATE nexus_build_options)

    # The upstream list remains authoritative, including app_max_size's source.
    add_subdirectory("${source}/boot/bootutil" upstream-bootutil)
    nexus_apply_target_policy(bootutil)
    target_include_directories(bootutil PRIVATE "${port}/port" "${port}/profile"
        "${source}/ext/tinycrypt/lib/include"
        "${source}/ext/mbedtls-asn1/include"
        "${source}/boot/zephyr/include")
    target_compile_definitions(bootutil PRIVATE MBEDTLS_CONFIG_FILE="config-asn1.h")
    target_link_libraries(bootutil PRIVATE "${flash}" mcuboot_crypto
        nexus_build_options)
    add_library(nexus_mcuboot_boot_storage INTERFACE)
    set_property(TARGET nexus_mcuboot_boot_storage PROPERTY
        INTERFACE_NEXUS_CRYPTO_ABI mcuboot-tinycrypt-asn1)
    set_property(TARGET nexus_mcuboot_boot_storage APPEND PROPERTY
        COMPATIBLE_INTERFACE_STRING NEXUS_CRYPTO_ABI)
    target_link_libraries(nexus_mcuboot_boot_storage INTERFACE bootutil)
    # The fixed verifier has no allocator: discard unselected supplier branches.
    target_link_options(nexus_mcuboot_boot_storage INTERFACE
        "LINKER:--gc-sections")
    target_include_directories(nexus_mcuboot_boot_storage INTERFACE
        "${port}/profile" "${port}/port")
    add_library(Nexus::MCUBootBootStorage ALIAS nexus_mcuboot_boot_storage)
endfunction()
