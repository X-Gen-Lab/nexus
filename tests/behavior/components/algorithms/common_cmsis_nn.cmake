# Execute the selected real upstream kernels through the public C adapter.
# Author: Nexus Team
nexus_add_cmsis_nn()
nexus_google_test(nexus_common_cmsis_nn_test
    SOURCES "${nexus_google_root}/components/algorithms/common_cmsis_nn_test.cpp"
    LIBRARIES Nexus::AlgorithmsCMSISNN
)
