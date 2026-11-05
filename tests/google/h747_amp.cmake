# Production C bridge; only exact H747 hardware observation/LL leaves mocked.
nexus_google_test(nexus_h747_amp_test
    SOURCES h747_amp_test.cpp
        "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32h747_amp/src/stm32h747_amp.c"
    LIBRARIES Nexus::AMPIPC Nexus::Arch
    INCLUDES
        "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32h747_amp/include"
        "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32h747_amp/private")

foreach(h747_core CM7 CM4)
    set(h747_target nexus_h747_amp_${h747_core}_hardware_test)
    nexus_google_test(${h747_target}
        SOURCES h747_amp_hardware_test.cpp
            "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32h747_amp/src/hardware.c"
        LIBRARIES Nexus::Core Nexus::Arch
        INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}"
            "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32h747_amp/private")
    target_include_directories(${h747_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
        "${NEXUS_SOURCE_DIR}/vendors/st/stm32h7_hal/Inc")
    nexus_use_cmsis_core(${h747_target})
    target_compile_definitions(${h747_target} PRIVATE STM32H747xx
        CORE_${h747_core} NX_H747_AMP_MODEL=1)
endforeach()
