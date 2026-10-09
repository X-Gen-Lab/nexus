# CMakeLists.txt
# Explicit static-only maintained FreeRTOS Cortex-M4F source target.
# Author: Nexus Team
set(kernel_root "${CMAKE_CURRENT_SOURCE_DIR}/../../ext/freertos")
add_library(nx_freertos_kernel STATIC
    "${kernel_root}/tasks.c"
    "${kernel_root}/queue.c"
    "${kernel_root}/list.c"
    "${kernel_root}/portable/GCC/ARM_CM4F/port.c")
target_include_directories(nx_freertos_kernel PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${kernel_root}/include"
    "${kernel_root}/portable/GCC/ARM_CM4F")
target_link_libraries(nx_freertos_kernel PRIVATE nexus_build_options)

add_library(nx_freertos_hooks OBJECT hooks.c)
target_include_directories(nx_freertos_hooks PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${kernel_root}/include"
    "${kernel_root}/portable/GCC/ARM_CM4F")
target_link_libraries(nx_freertos_hooks PRIVATE nexus_os_wait
    nexus_owned_warnings nexus_build_options)

add_library(nexus_os_freertos STATIC freertos.c)
target_sources(nexus_os_freertos INTERFACE $<TARGET_OBJECTS:nx_freertos_hooks>)
target_include_directories(nexus_os_freertos PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_os_freertos PUBLIC nexus_os_wait nx_freertos_kernel
    PRIVATE nexus_owned_warnings nexus_build_options)
