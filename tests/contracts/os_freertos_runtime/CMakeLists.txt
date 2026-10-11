# Real kernel POSIX execution; not a native API shim or MCU memory estimate.
set(kernel_root "${NEXUS_SOURCE_DIR}/ext/freertos")
add_library(contract_freertos_posix STATIC
    "${kernel_root}/tasks.c" "${kernel_root}/queue.c" "${kernel_root}/list.c"
    "${kernel_root}/portable/ThirdParty/GCC/Posix/port.c")
target_include_directories(contract_freertos_posix PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${kernel_root}/include"
    "${kernel_root}/portable/ThirdParty/GCC/Posix")
# POSIX/legacy usleep declarations without predefining the upstream __USE_GNU.
target_compile_definitions(contract_freertos_posix PRIVATE _DEFAULT_SOURCE)
target_link_libraries(contract_freertos_posix PUBLIC Threads::Threads)

# The real port cancels cond-waiting threads during task reclamation. Its event
# backend must unlock on POSIX cancellation before port.c signals and joins.
add_library(contract_freertos_posix_events OBJECT posix_event.c posix_idle.c)
target_include_directories(contract_freertos_posix_events PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}" "${kernel_root}/include"
    "${kernel_root}/portable/ThirdParty/GCC/Posix")
target_link_libraries(contract_freertos_posix_events PRIVATE
    Threads::Threads nexus_owned_warnings nexus_build_options)
target_sources(contract_freertos_posix PRIVATE
    $<TARGET_OBJECTS:contract_freertos_posix_events>)

add_library(contract_os_freertos_posix STATIC
    "${NEXUS_SOURCE_DIR}/os/freertos/freertos.c")
target_include_directories(contract_os_freertos_posix PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}" "${NEXUS_SOURCE_DIR}/os/freertos/include")
target_link_libraries(contract_os_freertos_posix PUBLIC
    contract_freertos_posix nexus_os_wait nexus_io_native
    PRIVATE nexus_arch nexus_owned_warnings nexus_build_options)
add_library(contract_os_freertos_hooks OBJECT "${NEXUS_SOURCE_DIR}/os/freertos/hooks.c")
target_include_directories(contract_os_freertos_hooks PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}" "${NEXUS_SOURCE_DIR}/os/freertos/include")
target_link_libraries(contract_os_freertos_hooks PRIVATE contract_freertos_posix nexus_os_wait nexus_owned_warnings)
target_sources(contract_os_freertos_posix INTERFACE $<TARGET_OBJECTS:contract_os_freertos_hooks>)
add_executable(contract_os_freertos "${CMAKE_CURRENT_SOURCE_DIR}/../os_freertos.c")
target_link_libraries(contract_os_freertos PRIVATE contract_os_freertos_posix nexus_owned_warnings)
add_test(NAME contract_os_freertos COMMAND contract_os_freertos)
set_tests_properties(contract_os_freertos PROPERTIES TIMEOUT 30)
