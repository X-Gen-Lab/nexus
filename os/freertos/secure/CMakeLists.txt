# No second kernel: real reviewed Secure context ASM plus explicit Nexus leases.
string(REGEX REPLACE "^cortex-m" "" secure_core "${NEXUS_CPU_ARCH}")
set(secure_port
    "${NEXUS_SOURCE_DIR}/ext/freertos/portable/GCC/ARM_CM${secure_core}/secure")
if(NOT EXISTS "${secure_port}/secure_context_port.c")
    message(FATAL_ERROR "Missing locked matching Secure assembly port")
endif()
# Objects enter the firmware directly so the linker can emit all NSC veneers;
# static archive extraction must not depend on a Secure caller using a gateway.
add_library(nx_freertos_secure_port OBJECT
    "${secure_port}/secure_context_port.c")
target_include_directories(nx_freertos_secure_port PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/config")
target_include_directories(nx_freertos_secure_port SYSTEM PRIVATE "${secure_port}")
target_link_libraries(nx_freertos_secure_port PRIVATE nexus_build_options)

add_library(nexus_secure_context OBJECT context.c init.c)
target_sources(nexus_secure_context INTERFACE
    $<TARGET_OBJECTS:nx_freertos_secure_port>)
target_include_directories(nexus_secure_context PUBLIC
    "${NEXUS_SOURCE_DIR}/os/freertos/include")
target_include_directories(nexus_secure_context BEFORE PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/config")
target_include_directories(nexus_secure_context SYSTEM PRIVATE "${secure_port}")
target_link_libraries(nexus_secure_context PUBLIC nexus_core nexus_arch
    PRIVATE nexus_build_options nexus_owned_warnings)
add_library(Nexus::SecureContext ALIAS nexus_secure_context)
