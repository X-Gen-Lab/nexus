# Execute the pinned kernel with its real POSIX port. This is not MCU/HIL evidence.
set(kernel "${CMAKE_SOURCE_DIR}/ext/freertos")
set(port "${kernel}/portable/ThirdParty/GCC/Posix")
add_library(freertos_contract_kernel STATIC
    "${kernel}/tasks.c" "${kernel}/queue.c" "${kernel}/list.c"
    "${kernel}/event_groups.c" "${kernel}/timers.c" "${kernel}/stream_buffer.c"
    "${kernel}/portable/MemMang/heap_4.c" "${port}/port.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/wait_for_event.c")
target_include_directories(freertos_contract_kernel PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}" "${kernel}/include" "${port}" "${port}/utils")
target_compile_definitions(freertos_contract_kernel PRIVATE _DEFAULT_SOURCE)
target_compile_options(freertos_contract_kernel PRIVATE -UNDEBUG)
target_link_libraries(freertos_contract_kernel PUBLIC Threads::Threads)

add_library(freertos_contract_adapter STATIC "${CMAKE_SOURCE_DIR}/osal/adapters/freertos/osal_freertos.c")
target_include_directories(freertos_contract_adapter PUBLIC "${CMAKE_SOURCE_DIR}/osal/include")
target_link_libraries(freertos_contract_adapter PUBLIC freertos_contract_kernel nexus_build_options)
target_compile_options(freertos_contract_adapter PRIVATE -Werror -UNDEBUG)
add_executable(freertos_contract_tests test_freertos_contracts.c)
target_link_libraries(freertos_contract_tests PRIVATE freertos_contract_adapter)
target_compile_options(freertos_contract_tests PRIVATE -Werror -UNDEBUG)
add_test(NAME freertos_contract_tests COMMAND freertos_contract_tests)
set_tests_properties(freertos_contract_tests PROPERTIES TIMEOUT 60 LABELS "osal;freertos;kernel;contract")

# Keep the local pthread cancellation repair under an executable contract.
add_executable(posix_event_contract_tests test_posix_event.c wait_for_event.c)
target_include_directories(posix_event_contract_tests PRIVATE "${port}/utils")
target_link_libraries(posix_event_contract_tests PRIVATE Threads::Threads nexus_build_options)
target_compile_definitions(posix_event_contract_tests PRIVATE _POSIX_C_SOURCE=200809L)
target_compile_options(posix_event_contract_tests PRIVATE -UNDEBUG -Werror)
add_test(NAME posix_event_contract_tests COMMAND posix_event_contract_tests)
set_tests_properties(posix_event_contract_tests PROPERTIES TIMEOUT 15 LABELS "osal;freertos;host-port;contract")
