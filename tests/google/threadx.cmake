# Actual pinned Linux kernel and production algorithms; no MCU test runtime.
include("${NEXUS_SOURCE_DIR}/os/threadx/Kernel.cmake")
nexus_add_threadx_kernel(contract_threadx_linux linux 1000 0)
set(tx_root "${NEXUS_SOURCE_DIR}/os/threadx")
set(tx_fixture "${NEXUS_SOURCE_DIR}/tests/contracts/os_threadx_runtime")
set(tx_service_sources "${tx_root}/threadx.c" "${tx_fixture}/clock.c")
set(tx_service_includes "${tx_root}/include"
    "${NEXUS_SOURCE_DIR}/arch/include")
set(tx_service_libraries contract_threadx_linux Nexus::OSWait Nexus::Arch)

nexus_google_test(nexus_threadx_kernel_test
    SOURCES threadx_kernel_test.cpp threadx_close_test.cpp
        threadx_wait_test.cpp threadx_admission_test.cpp ${tx_service_sources}
    LIBRARIES ${tx_service_libraries}
    INCLUDES ${tx_service_includes})
target_link_options(nexus_threadx_kernel_test PRIVATE
    -Wl,--wrap=pthread_mutex_timedlock)

nexus_google_test(nexus_threadx_bootstrap_test
    SOURCES threadx_bootstrap_test.cpp ${tx_service_sources}
    LIBRARIES ${tx_service_libraries}
    INCLUDES ${tx_service_includes})
target_link_options(nexus_threadx_bootstrap_test PRIVATE
    -Wl,--wrap=_tx_initialize_low_level -Wl,--wrap=nx_arch_in_isr
    -Wl,--wrap=nx_arch_is_privileged -Wl,--wrap=nx_arch_irq_masks)

foreach(policy basepri primask)
    if(policy STREQUAL "basepri")
        set(suffix "")
        set(definitions TX_PORT_USE_BASEPRI TX_PORT_BASEPRI=80)
        set(config "${tx_fixture}/m4-config")
    else()
        set(suffix "_primask")
        set(definitions)
        set(config "${tx_fixture}/m4-primask-config")
    endif()
    set(m4_target "nexus_threadx_m4${suffix}_test")
    nexus_google_test(${m4_target}
        SOURCES "threadx_m4${suffix}_test.cpp" "${tx_root}/m4.c"
        LIBRARIES contract_threadx_linux Nexus::OSWait
        INCLUDES "${tx_root}" "${tx_root}/include" "${tx_root}/private"
            "${config}")
    target_compile_definitions(${m4_target} PRIVATE
        NEXUS_THREADX_M4_MODEL ${definitions})

    set(irq_target "nexus_threadx_irq${suffix}_test")
    nexus_google_test(${irq_target}
        SOURCES "threadx_irq${suffix}_test.cpp" ${tx_service_sources}
        LIBRARIES ${tx_service_libraries}
        INCLUDES ${tx_service_includes} "${tx_root}" "${tx_root}/private")
    target_compile_definitions(${irq_target} PRIVATE
        NEXUS_THREADX_IRQ_MODEL NEXUS_CPU_EXTERNAL_IRQ_COUNT=82
        NEXUS_IRQ_PRIORITY_BITS=4 ${definitions})
    target_link_options(${irq_target} PRIVATE
        -Wl,--wrap=nx_arch_is_privileged -Wl,--wrap=nx_arch_irq_is_masked
        -Wl,--wrap=nx_arch_exception_number)
endforeach()

nexus_google_test(nexus_threadx_m4_external_test
    SOURCES threadx_m4_external_test.cpp "${tx_root}/m4.c"
    LIBRARIES contract_threadx_linux Nexus::OSWait
    INCLUDES "${tx_root}" "${tx_root}/include" "${tx_root}/private"
        "${tx_fixture}/m4-external-config")
target_compile_definitions(nexus_threadx_m4_external_test PRIVATE
    NEXUS_THREADX_M4_MODEL TX_PORT_USE_BASEPRI TX_PORT_BASEPRI=80)

nexus_google_test(nexus_threadx_linux_port_test
    SOURCES threadx_linux_port_test.cpp
    LIBRARIES contract_threadx_linux)
target_link_options(nexus_threadx_linux_port_test PRIVATE
    -Wl,--wrap=nice -Wl,--wrap=sem_wait -Wl,--wrap=sem_post
    -Wl,--wrap=sigsuspend -Wl,--wrap=sigwait -Wl,--wrap=clock_gettime
    -Wl,--wrap=sem_timedwait -Wl,--wrap=sem_clockwait
    -Wl,--wrap=_tx_thread_context_save -Wl,--wrap=_tx_timer_interrupt
    -Wl,--wrap=_tx_thread_context_restore)
