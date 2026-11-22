# Contract tests for selected pure C algorithms and runtime helpers.
# Author: Nexus Team
nexus_google_test(nexus_common_algorithms_test
    SOURCES "${nexus_google_root}/components/common_algorithms_test.cpp"
    INCLUDES "${NEXUS_SOURCE_DIR}/components/algorithms/include"
    LIBRARIES Nexus::Algorithms
)
nexus_google_test(nexus_common_motion_test
    SOURCES "${nexus_google_root}/components/common_motion_test.cpp"
    INCLUDES "${NEXUS_SOURCE_DIR}/components/algorithms/include"
    LIBRARIES Nexus::Algorithms
)
nexus_google_test(nexus_common_fsm_test
    SOURCES "${nexus_google_root}/components/common_fsm_test.cpp"
    LIBRARIES Nexus::FSM
)
nexus_google_test(nexus_common_debounce_test
    SOURCES "${nexus_google_root}/components/common_debounce_test.cpp"
    LIBRARIES Nexus::Debounce
)
nexus_google_test(nexus_common_runtime_test
    SOURCES "${nexus_google_root}/components/common_runtime_test.cpp"
    LIBRARIES Nexus::Periodic Nexus::Timers Nexus::EventFanout Nexus::IO
)
nexus_add_cmsis_dsp()
nexus_google_test(nexus_common_cmsis_dsp_test
    SOURCES "${nexus_google_root}/components/common_cmsis_dsp_test.cpp"
    LIBRARIES Nexus::AlgorithmsCMSISDSP
)
nexus_google_test(nexus_common_owner_integration_test
    SOURCES "${nexus_google_root}/components/common_owner_integration_test.cpp"
    LIBRARIES Nexus::Periodic Nexus::OwnerObserver
)
