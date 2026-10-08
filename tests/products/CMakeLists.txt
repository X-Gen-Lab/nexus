add_executable(product_boot_contract_tests
    test_boot.c "${NEXUS_SOURCE_DIR}/products/src/product.c")
target_include_directories(product_boot_contract_tests PRIVATE
    "${NEXUS_SOURCE_DIR}/products/include")
target_link_libraries(product_boot_contract_tests PRIVATE hal_interface osal_interface)
if(NOT MSVC)
    target_compile_options(product_boot_contract_tests PRIVATE -Werror -UNDEBUG)
endif()
add_test(NAME product_boot_contract_tests COMMAND product_boot_contract_tests)
set_tests_properties(product_boot_contract_tests PROPERTIES
    LABELS "product;boot;contract" TIMEOUT 10)

nexus_add_application(TARGET product_native_smoke SOURCES test_native.c)
if(NOT MSVC)
    target_compile_options(product_native_smoke PRIVATE -Werror -UNDEBUG)
endif()
add_test(NAME product_native_smoke COMMAND product_native_smoke)
set_tests_properties(product_native_smoke PROPERTIES
    LABELS "product;native;contract" TIMEOUT 10)
