add_executable(runtime_boot_contract_tests
    test_boot.c "${NEXUS_SOURCE_DIR}/runtime/src/nx_runtime.c")
target_include_directories(runtime_boot_contract_tests PRIVATE
    "${NEXUS_SOURCE_DIR}/runtime/include")
target_link_libraries(runtime_boot_contract_tests PRIVATE hal_interface osal_interface)
if(NOT MSVC)
    target_compile_options(runtime_boot_contract_tests PRIVATE -Werror -UNDEBUG)
endif()
add_test(NAME runtime_boot_contract_tests COMMAND runtime_boot_contract_tests)
set_tests_properties(runtime_boot_contract_tests PROPERTIES
    LABELS "runtime;boot;contract" TIMEOUT 10)

nexus_add_application(TARGET runtime_native_smoke SOURCES test_native.c)
if(NOT MSVC)
    target_compile_options(runtime_native_smoke PRIVATE -Werror -UNDEBUG)
endif()
add_test(NAME runtime_native_smoke COMMAND runtime_native_smoke)
set_tests_properties(runtime_native_smoke PROPERTIES
    LABELS "runtime;native;contract" TIMEOUT 10)
