# The selected CPU implementation has no vendor or kernel include surface.
if(NEXUS_CPU_ARCH STREQUAL "native")
    set(nexus_arch_port native)
elseif(NEXUS_CPU_ARCH MATCHES "^cortex-m(0|0plus|3|4|7|23|33|55|85)$")
    set(nexus_arch_port cortex_m)
else()
    message(FATAL_ERROR "Unsupported CPU architecture: ${NEXUS_CPU_ARCH}")
endif()
add_library(nexus_arch STATIC "${nexus_arch_port}/nx_arch_${nexus_arch_port}.c")
target_sources(nexus_arch PRIVATE cortex_m/nx_arch_cache.c
    cortex_m/nx_arch_mpu.c cortex_m/nx_arch_security.c
    cortex_m/nx_arch_sleep.c)
target_include_directories(nexus_arch PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_arch PUBLIC nexus_abi_options
    PRIVATE nexus_build_options nexus_owned_warnings)
set(nexus_arch_capabilities NEXUS_ARCH_HAS_DWT_CYCCNT
    NEXUS_ARCH_DCACHE_LINE_BYTES NEXUS_ARCH_ICACHE_LINE_BYTES
    NEXUS_ARCH_MPU_VERSION NEXUS_ARCH_SECURITY_STATE NEXUS_ARCH_HAS_SAU)
foreach(capability IN LISTS nexus_arch_capabilities)
    if(nexus_arch_port STREQUAL "native")
        if(DEFINED ${capability} AND NOT "${${capability}}" STREQUAL "0")
            message(FATAL_ERROR "Native cannot declare CPU hardware: ${capability}")
        endif()
        set(${capability} 0)
    elseif(NOT DEFINED ${capability})
        message(FATAL_ERROR "Missing reviewed CPU capability: ${capability}")
    endif()
    target_compile_definitions(nexus_arch PRIVATE
        ${capability}=${${capability}})
endforeach()
if(NOT NEXUS_ARCH_HAS_DWT_CYCCNT MATCHES "^[01]$" OR
        NOT NEXUS_ARCH_HAS_SAU MATCHES "^[01]$" OR
        NOT NEXUS_ARCH_DCACHE_LINE_BYTES MATCHES "^(0|32)$" OR
        NOT NEXUS_ARCH_ICACHE_LINE_BYTES MATCHES "^(0|32)$" OR
        NOT NEXUS_ARCH_MPU_VERSION MATCHES "^(0|7|8)$" OR
        NOT NEXUS_ARCH_SECURITY_STATE MATCHES "^[012]$")
    message(FATAL_ERROR "Invalid reviewed CPU mechanism capability")
endif()
if(nexus_arch_port STREQUAL "native" AND NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(nexus_arch PRIVATE Threads::Threads)
endif()
add_library(Nexus::Arch ALIAS nexus_arch)
