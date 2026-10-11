# CMakeLists.txt
# Explicit static-only maintained FreeRTOS CPU port source targets.
# Author: Nexus Team
set(kernel_root "${CMAKE_CURRENT_SOURCE_DIR}/../../ext/freertos")
set(maintained_ports
    GCC/ARM_CM0 GCC/ARM_CM3 GCC/ARM_CM4F GCC/ARM_CM7/r0p1
    GCC/ARM_CM3_MPU GCC/ARM_CM4_MPU
    GCC/ARM_CM23_NTZ/non_secure GCC/ARM_CM33_NTZ/non_secure
    GCC/ARM_CM55_NTZ/non_secure GCC/ARM_CM85_NTZ/non_secure
    GCC/ARM_CM23/non_secure GCC/ARM_CM33/non_secure
    GCC/ARM_CM55/non_secure GCC/ARM_CM85/non_secure
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
list(APPEND ports_with_assembly
    GCC/ARM_CM23/non_secure GCC/ARM_CM33/non_secure
    GCC/ARM_CM55/non_secure GCC/ARM_CM85/non_secure)
if(NEXUS_FREERTOS_PORT IN_LIST ports_with_assembly)
    list(APPEND port_sources "${port_root}/portasm.c")
endif()
if(NEXUS_OS_MEMORY_PROTECTION)
    # A reviewed derived TU grants no default unprivileged MMIO. Narrow SVC
    # veneers retain the exact two pinned implementations and no object pool.
    set(mpu_prepare "${CMAKE_CURRENT_SOURCE_DIR}/mpu/prepare_port.py")
    set(mpu_output "${CMAKE_CURRENT_BINARY_DIR}/mpu-port")
    set(mpu_veneers "${port_root}/mpu_wrappers_v2_asm.c")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${mpu_prepare}" "${mpu_veneers}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B "${mpu_prepare}"
        --source "${mpu_veneers}" --output "${mpu_output}/veneers.c" --veneers
        RESULT_VARIABLE mpu_result
        OUTPUT_VARIABLE mpu_stdout ERROR_VARIABLE mpu_stderr)
    if(NOT mpu_result EQUAL 0)
        message(FATAL_ERROR "Narrow MPU veneer derivation rejected: "
            "${mpu_stdout}${mpu_stderr}")
    endif()
    list(APPEND port_sources "${mpu_output}/veneers.c")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${port_root}/port.c")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B "${mpu_prepare}"
        --source "${port_root}/port.c" --output "${mpu_output}/port.c"
        RESULT_VARIABLE mpu_result
        OUTPUT_VARIABLE mpu_stdout ERROR_VARIABLE mpu_stderr)
    if(NOT mpu_result EQUAL 0)
        message(FATAL_ERROR "MPU bootstrap/peripheral derivation rejected: "
            "${mpu_stdout}${mpu_stderr}")
    endif()
    list(REMOVE_ITEM port_sources "${port_root}/port.c")
    list(APPEND port_sources "${mpu_output}/port.c")
    if(NEXUS_FREERTOS_PORT IN_LIST ports_with_assembly)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${port_root}/portasm.c")
        execute_process(COMMAND "${Python3_EXECUTABLE}" -B "${mpu_prepare}"
            --source "${port_root}/portasm.c" --output "${mpu_output}/portasm.c"
            --assembly RESULT_VARIABLE mpu_result
            OUTPUT_VARIABLE mpu_stdout ERROR_VARIABLE mpu_stderr)
        if(NOT mpu_result EQUAL 0)
            message(FATAL_ERROR "MPU bootstrap assembly derivation rejected: "
                "${mpu_stdout}${mpu_stderr}")
        endif()
        list(REMOVE_ITEM port_sources "${port_root}/portasm.c")
        list(APPEND port_sources "${mpu_output}/portasm.c")
    endif()
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
target_link_libraries(nx_freertos_kernel PRIVATE nexus_core nexus_build_options)
if(NEXUS_OS_MEMORY_PROTECTION)
    target_include_directories(nx_freertos_kernel PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/mpu")
endif()
if(NEXUS_OS_MEMORY_PROTECTION AND
        NEXUS_FREERTOS_PORT STREQUAL "GCC/ARM_CM23_NTZ/non_secure")
    # The locked baseline MPU SVC implementation uses unified Thumb syntax.
    target_compile_options(nx_freertos_kernel PRIVATE -masm-syntax-unified)
endif()
if(NEXUS_OS_TRUSTZONE)
    string(REGEX REPLACE "^cortex-m" "" secure_core "${NEXUS_CPU_ARCH}")
    # Matching Secure port declarations are implementation details of the
    # Nonsecure kernel; the public consumer receives no Secure config path.
    target_include_directories(nx_freertos_kernel SYSTEM PRIVATE
        "${kernel_root}/portable/GCC/ARM_CM${secure_core}/secure")
endif()

add_library(nx_freertos_hooks OBJECT hooks.c)
target_include_directories(nx_freertos_hooks PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${kernel_root}/include"
    "${port_root}")
target_link_libraries(nx_freertos_hooks PRIVATE nexus_os_wait
    nexus_owned_warnings nexus_build_options)

add_library(nexus_os_freertos STATIC freertos.c)
if(NEXUS_OS_MEMORY_PROTECTION)
    target_sources(nexus_os_freertos PRIVATE mpu/task.c mpu/layout.c)
    # Port SVC code can require this table after scanning the adapter archive.
    # Enter it directly before all static archives, without an object pool.
    add_library(nx_freertos_mpu_syscalls OBJECT mpu/syscalls.c
        mpu/guard.c mpu/guard_arm.c)
    target_include_directories(nx_freertos_mpu_syscalls PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${kernel_root}/include" "${port_root}")
    target_link_libraries(nx_freertos_mpu_syscalls PRIVATE nexus_core nexus_arch
        nexus_owned_warnings nexus_build_options)
    target_sources(nexus_os_freertos INTERFACE
        $<TARGET_OBJECTS:nx_freertos_mpu_syscalls>)
endif()
# Kernel-owned callbacks must precede static archives at the consumer link.
# Otherwise the adapter archive is scanned before tasks.c requires its hooks.
# Unreferenced default-profile sections are still removed by --gc-sections.
add_library(nx_freertos_optional_hooks OBJECT lowpower.c diagnostics.c)
target_include_directories(nx_freertos_optional_hooks PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${kernel_root}/include" "${port_root}")
target_link_libraries(nx_freertos_optional_hooks PRIVATE nexus_os_wait
    nexus_arch nexus_owned_warnings nexus_build_options)
if(NEXUS_OS_MEMORY_PROTECTION)
    target_compile_definitions(nx_freertos_optional_hooks PRIVATE
        MPU_WRAPPERS_INCLUDED_FROM_API_FILE)
endif()
target_sources(nexus_os_freertos INTERFACE
    $<TARGET_OBJECTS:nx_freertos_optional_hooks>)
if(NEXUS_OS_EXTERNAL_TICK)
    add_library(nx_freertos_tick OBJECT tick.c)
    target_include_directories(nx_freertos_tick PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${kernel_root}/include" "${port_root}")
    target_link_libraries(nx_freertos_tick PRIVATE nexus_core
        nexus_owned_warnings nexus_build_options)
    target_sources(nexus_os_freertos INTERFACE
        $<TARGET_OBJECTS:nx_freertos_tick>)
endif()
target_sources(nexus_os_freertos INTERFACE $<TARGET_OBJECTS:nx_freertos_hooks>)
target_include_directories(nexus_os_freertos PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_os_freertos PUBLIC nexus_os_wait nx_freertos_kernel
    PRIVATE nexus_arch nexus_owned_warnings nexus_build_options)
