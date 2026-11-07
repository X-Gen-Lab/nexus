# Actual common controller and two production register specializations.
nexus_google_test(nexus_can_controller_test SOURCES can_controller_test.cpp
    LIBRARIES Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers"
        "${NEXUS_SOURCE_DIR}/io/private")
foreach(can_family stm32)
    set(can_target "nexus_${can_family}_can_test")
    nexus_google_test(${can_target} SOURCES can_soc_test.cpp
        LIBRARIES Nexus::NativeModel Nexus::Arch
        INCLUDES "${NEXUS_SOURCE_DIR}/tests/support/native/drivers"
            "${NEXUS_SOURCE_DIR}/io/private")
    target_compile_options(${can_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/can_soc_model.h")
    target_sources(${can_target} PRIVATE
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/can.c")
    target_include_directories(${can_target} PRIVATE
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
    target_include_directories(${can_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    nexus_use_cmsis_core(${can_target})
    target_compile_definitions(${can_target} PRIVATE
        NX_CAN_SOC_STM32 NEXUS_STM32_MODEL STM32F407xx)
endforeach()
