# Normal canonical assemblies, real System/timebase/GPIO and physical DMA spans.
foreach(eth_frames_family stm32)
    set(eth_frames_label f407zg-eth-frames)
    set(eth_frames_soc stm32f4)
    set(eth_frames_input
        "${NEXUS_SOURCE_DIR}/cmake/package/verification/io_evolution/${eth_frames_label}.toml")
    set(eth_frames_bundle
        "${CMAKE_CURRENT_BINARY_DIR}/eth-frames-factory-${eth_frames_family}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py" --test-support
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${eth_frames_input}"
        --output "${eth_frames_bundle}" RESULT_VARIABLE eth_frames_generated
        OUTPUT_VARIABLE eth_frames_output ERROR_VARIABLE eth_frames_error)
    if(NOT eth_frames_generated EQUAL 0)
        message(FATAL_ERROR
            "Enhanced Ethernet assembly rejected: ${eth_frames_output}${eth_frames_error}")
    endif()
    nexus_google_configuration_inputs("${eth_frames_bundle}")
    set(eth_frames_target
        "nexus_${eth_frames_family}_eth_frames_factory_test")
    nexus_google_test(${eth_frames_target}
        SOURCES ethernet_frames_factory_test.cpp "${eth_frames_bundle}/bindings.c"
            "${NEXUS_SOC_SOURCE_${eth_frames_soc}}/system.c"
            "${NEXUS_SOC_SOURCE_${eth_frames_soc}}/drivers/gpio.c"
            "${NEXUS_SOC_SOURCE_${eth_frames_soc}}/drivers/ethernet_frames.c"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ip/legacy_mac/eth_frames_mac.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${eth_frames_bundle}" "${NEXUS_SOURCE_DIR}/io/private"
            "${NEXUS_SOC_SOURCE_${eth_frames_soc}}"
            "${NEXUS_SOC_SOURCE_${eth_frames_soc}}/private")
    target_compile_options(${eth_frames_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/ethernet_frames_factory_model.h"
        -fdata-sections -ffunction-sections -fno-pie)
    target_compile_definitions(${eth_frames_target} PRIVATE
        _init=nx_eth_frames_fixture_init _fini=nx_eth_frames_fixture_fini)
    target_link_options(${eth_frames_target} PRIVATE -no-pie
        "-Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/eth_frames_host_dma.ld")
    set_property(TARGET ${eth_frames_target} APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/eth_frames_host_dma.ld")
    target_sources(${eth_frames_target} PRIVATE
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_cold.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_lifecycle.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/platform_coordinator.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/system_lifecycle.c")
    # Model excludes reset-only CPU assembly, never the actual clock/timer
    # transaction. CPP binds the real System context to the same MMIO map.
    target_compile_definitions(${eth_frames_target} PRIVATE
        NX_ETH_FRAMES_FACTORY_STM32 NEXUS_STM32_MODEL STM32F407xx
        USE_RTOS=0 USE_HAL_ETH_REGISTER_CALLBACKS=0)
    target_include_directories(${eth_frames_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    nexus_use_cmsis_core(${eth_frames_target})
    nexus_stm32f4_official(${eth_frames_target})
endforeach()
