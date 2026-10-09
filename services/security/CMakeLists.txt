# Provider-independent capability facade; no OpenSSL or board dependencies.
add_library(nexus_security STATIC src/crypto.c)
target_include_directories(nexus_security PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_security PUBLIC nexus_build_options)

# The effective configuration selects an optional implementation target.
# Startup selection is a separate, explicit nx_crypto_set_provider call.
if(CONFIG_CRYPTO_PROVIDER_OPENSSL)
    if(NOT NEXUS_PLATFORM STREQUAL "native")
        message(FATAL_ERROR "OpenSSL crypto provider requires the Native platform")
    endif()
    if(NOT TARGET OpenSSL::Crypto)
        set(nexus_openssl_created_here TRUE)
    endif()
    find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto)
    if(nexus_openssl_created_here)
        set_property(TARGET OpenSSL::Crypto PROPERTY IMPORTED_GLOBAL TRUE)
    endif()
    add_library(nexus_security_openssl STATIC src/crypto_openssl.c)
    target_link_libraries(nexus_security_openssl PUBLIC nexus_security PRIVATE OpenSSL::Crypto)
endif()
