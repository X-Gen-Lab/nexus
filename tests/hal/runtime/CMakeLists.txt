# Compile the real dispatcher and Native Arch, overriding only the ISR probe.
add_executable(hal_completion_tests test_completion.cpp
    "${NEXUS_SOURCE_DIR}/hal/runtime/nx_completion.c")
target_compile_definitions(hal_completion_tests PRIVATE
    nx_arch_in_isr=completion_test_in_isr)
target_compile_features(hal_completion_tests PRIVATE cxx_std_17)
target_link_libraries(hal_completion_tests PRIVATE
    hal_interface nexus_arch GTest::gtest_main ${THREAD_LIBS})
if(NOT MSVC)
    target_compile_options(hal_completion_tests PRIVATE -Wall -Wextra -Werror)
endif()
nexus_discover_tests(hal_completion_tests PROPERTIES
    TIMEOUT 10 LABELS "hal;runtime;completion;contract")

# A plain C consumer must link this component without HALCore/UART or OSAL.
add_executable(hal_completion_minimal_link test_minimal_completion.c)
target_link_libraries(hal_completion_minimal_link PRIVATE Nexus::HALCompletion)
add_test(NAME hal_completion_minimal_link COMMAND hal_completion_minimal_link)
set_tests_properties(hal_completion_minimal_link PROPERTIES
    TIMEOUT 10 LABELS "hal;runtime;completion;minimal;contract")
