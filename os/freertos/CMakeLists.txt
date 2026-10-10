# CMakeLists.txt
# Explicit static-only maintained FreeRTOS CPU port source targets.
# Author: Nexus Team
set(kernel_root "${CMAKE_CURRENT_SOURCE_DIR}/../../ext/freertos")
set(maintained_ports
    GCC/ARM_CM0 GCC/ARM_CM3 GCC/ARM_CM4F GCC/ARM_CM7/r0p1
    GCC/ARM_CM23_NTZ/non_secure GCC/ARM_CM33_NTZ/non_secure
    GCC/ARM_CM55_NTZ/non_secure GCC/ARM_CM85_NTZ/non_secure
    nexus/ARM_CM7_integer)
if(NOT NEXUS_FREERTOS_PORT IN_LIST maintained_ports)
    message(FATAL_ERROR "Unsupported maintained FreeRTOS port contract")
endif()
if(NEXUS_FREERTOS_PORT STREQUAL "nexus/ARM_CM7_integer")
    set(port_root "${CMAKE_CURRENT_BINARY_DIR}/m7-integer-port")
    set(overlay "${CMAKE_CURRENT_SOURCE_DIR}/prepare_m7_integer.py")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${overlay}" "${kernel_root}/portable/GCC/ARM_CM3/port.c"
        "${kernel_root}/portable/GCC/ARM_CM3/portmacro.h")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B "${overlay}"
        --kernel-root "${kernel_root}" --output "${port_root}"
        RESULT_VARIABLE overlay_result
        OUTPUT_VARIABLE overlay_output ERROR_VARIABLE overlay_error)
    if(NOT overlay_result EQUAL 0)
        message(FATAL_ERROR "Integer M7 port overlay rejected: "
            "${overlay_output}${overlay_error}")
    endif()
else()
    set(port_root "${kernel_root}/portable/${NEXUS_FREERTOS_PORT}")
endif()
set(port_sources "${port_root}/port.c")
set(ports_with_assembly
    GCC/ARM_CM0 GCC/ARM_CM23_NTZ/non_secure GCC/ARM_CM33_NTZ/non_secure
    GCC/ARM_CM55_NTZ/non_secure GCC/ARM_CM85_NTZ/non_secure)
if(NEXUS_FREERTOS_PORT IN_LIST ports_with_assembly)
    list(APPEND port_sources "${port_root}/portasm.c")
endif()
add_library(nx_freertos_kernel STATIC
    "${kernel_root}/tasks.c"
    "${kernel_root}/queue.c"
    "${kernel_root}/list.c"
    ${port_sources})
target_include_directories(nx_freertos_kernel PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${kernel_root}/include"
    "${port_root}")
target_link_libraries(nx_freertos_kernel PRIVATE nexus_build_options)

add_library(nx_freertos_hooks OBJECT hooks.c)
target_include_directories(nx_freertos_hooks PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${kernel_root}/include"
    "${port_root}")
target_link_libraries(nx_freertos_hooks PRIVATE nexus_os_wait
    nexus_owned_warnings nexus_build_options)

add_library(nexus_os_freertos STATIC freertos.c)
target_sources(nexus_os_freertos INTERFACE $<TARGET_OBJECTS:nx_freertos_hooks>)
target_include_directories(nexus_os_freertos PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_os_freertos PUBLIC nexus_os_wait nx_freertos_kernel
    PRIVATE nexus_arch nexus_owned_warnings nexus_build_options)
