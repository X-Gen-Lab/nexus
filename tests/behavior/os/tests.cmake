# Explicit os behavior targets and private model requirements.
# Author: Nexus Team
include("${nexus_google_root}/os/freertos_stop.cmake")

nexus_google_test(nexus_irq_wake_provider_test SOURCES "${nexus_google_root}/os/irq_wake_test.cpp"
    LIBRARIES Nexus::NativeModel
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers"
)

nexus_google_test(nexus_freertos_services_kernel_test
    SOURCES "${nexus_google_root}/os/freertos_services_kernel_test.cpp" "${nexus_google_root}/os/freertos_stack_kernel_test.cpp"
        "${nexus_google_root}/os/freertos_owner_endurance_test.cpp"
        "${NEXUS_SOURCE_DIR}/os/freertos/stack_snapshot.c"
    LIBRARIES contract_os_freertos_posix Nexus::UARTOwner Nexus::NativeModel
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers"
)

include(
    "${nexus_google_root}/components/services/common_diagnostics_freertos.cmake"
)

nexus_google_test(nexus_irq_wake_stm32_test STM32F4_LL SOURCES "${nexus_google_root}/os/irq_wake_stm32_test.cpp"
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/uart/uart.c"
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4"
)

target_include_directories(
    nexus_irq_wake_stm32_test
    SYSTEM
    PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}"
)

nexus_use_cmsis_core(nexus_irq_wake_stm32_test)

target_compile_definitions(
    nexus_irq_wake_stm32_test
    PRIVATE NEXUS_STM32_MODEL STM32F407xx
)

include("${nexus_google_root}/os/freertos_context.cmake")

include("${nexus_google_root}/os/freertos_prestart.cmake")

include("${nexus_google_root}/os/os_wait.cmake")

include("${nexus_google_root}/os/os_portable.cmake")

nexus_google_test(nexus_wait_select_test SOURCES "${nexus_google_root}/os/wait_select_test.cpp"
    LIBRARIES Nexus::OSWait
)

include("${nexus_google_root}/os/os_optional.cmake")

include("${nexus_google_root}/os/os_secure.cmake")

include("${nexus_google_root}/os/freertos_mpu.cmake")

include("${nexus_google_root}/os/timebase_lowpower.cmake")

include("${nexus_google_root}/os/threadx.cmake")
