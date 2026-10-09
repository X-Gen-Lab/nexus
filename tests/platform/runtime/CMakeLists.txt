add_executable(runtime_boot_contract_tests
    test_boot.c "${NEXUS_SOURCE_DIR}/runtime/src/nx_runtime.c")
target_include_directories(runtime_boot_contract_tests PRIVATE
    "${NEXUS_SOURCE_DIR}/runtime/include")
target_link_libraries(runtime_boot_contract_tests PRIVATE hal_interface osal_interface nexus_arch_interface)
if(NOT MSVC)
    target_compile_options(runtime_boot_contract_tests PRIVATE -Werror -UNDEBUG)
endif()
add_test(NAME runtime_boot_contract_tests COMMAND runtime_boot_contract_tests)
set_tests_properties(runtime_boot_contract_tests PROPERTIES
    LABELS "runtime;boot;contract" TIMEOUT 10)

nexus_add_application(TARGET runtime_native_smoke SOURCES test_native.c)
target_link_libraries(runtime_native_smoke PRIVATE Nexus::Arch)
if(NOT MSVC)
    target_compile_options(runtime_native_smoke PRIVATE -Werror -UNDEBUG)
endif()
add_test(NAME runtime_native_smoke COMMAND runtime_native_smoke)
set_tests_properties(runtime_native_smoke PROPERTIES
    LABELS "runtime;native;contract" TIMEOUT 10)

foreach(variant model native)
    add_executable(hal_lifecycle_${variant}_context_tests
        test_hal_context.c "${NEXUS_SOURCE_DIR}/hal/src/nx_hal.c")
    target_link_libraries(hal_lifecycle_${variant}_context_tests PRIVATE
        hal_interface osal_interface nexus_arch_interface)
    if(variant STREQUAL "native")
        target_compile_definitions(hal_lifecycle_${variant}_context_tests PRIVATE
            NX_HAL_CONTEXT_REAL_ARCH=1)
        target_link_libraries(hal_lifecycle_${variant}_context_tests PRIVATE Nexus::Arch)
    endif()
    if(NOT MSVC)
        target_compile_options(hal_lifecycle_${variant}_context_tests PRIVATE -Werror -UNDEBUG)
    endif()
    add_test(NAME hal_lifecycle_${variant}_context_tests
        COMMAND hal_lifecycle_${variant}_context_tests)
    set_tests_properties(hal_lifecycle_${variant}_context_tests PROPERTIES
        LABELS "hal;runtime;${variant};context" TIMEOUT 10)
endforeach()
