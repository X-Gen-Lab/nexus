# Raw capture stays independent from Kernel, transport and persistent media.
# Author: Nexus Team
add_library(
    nexus_fault_record
    STATIC
    EXCLUDE_FROM_ALL
    src/fault_record.c
    src/fault_m4.c
)
target_include_directories(
    nexus_fault_record
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    nexus_fault_record
    PUBLIC nexus_core
    PRIVATE nexus_arch nexus_owned_warnings nexus_build_options
)
add_library(Nexus::Fault ALIAS nexus_fault_record)
nexus_apply_target_policy(nexus_fault_record)
get_target_property(fault_record_cpu Nexus::Config NEXUS_CPU_ARCH)
if(fault_record_cpu STREQUAL "cortex-m4")
    target_compile_options(nexus_fault_record PRIVATE -mgeneral-regs-only)
endif()

add_library(nexus_fault_storage STATIC EXCLUDE_FROM_ALL src/fault_storage.c)
target_link_libraries(
    nexus_fault_storage
    PUBLIC nexus_fault_record nexus_storage
    PRIVATE nexus_arch nexus_owned_warnings nexus_build_options
)
add_library(Nexus::FaultStorage ALIAS nexus_fault_storage)
nexus_apply_target_policy(nexus_fault_storage)

include("${CMAKE_CURRENT_SOURCE_DIR}/Profiles.cmake")
