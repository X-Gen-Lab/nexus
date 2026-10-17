# Normal canonical assemblies, real System/timebase/GPIO and physical DMA spans.
foreach(eth_frames_family stm32 gd32)
    if(eth_frames_family STREQUAL "stm32")
        set(eth_frames_label f407zg-eth-frames)
        set(eth_frames_soc stm32f407)
    else()
        set(eth_frames_label gd470-eth-frames)
        set(eth_frames_soc gd32f470)
    endif()
    set(eth_frames_input
        "${NEXUS_SOURCE_DIR}/cmake/package/verification/io_evolution/${eth_frames_label}.toml")
    set(eth_frames_bundle
        "${CMAKE_CURRENT_BINARY_DIR}/eth-frames-factory-${eth_frames_family}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${eth_frames_input}"
        --output "${eth_frames_bundle}" RESULT_VARIABLE eth_frames_generated
        OUTPUT_VARIABLE eth_frames_output ERROR_VARIABLE eth_frames_error)
    if(NOT eth_frames_generated EQUAL 0)
        message(FATAL_ERROR
            "Enhanced Ethernet assembly rejected: ${eth_frames_output}${eth_frames_error}")
    endif()
    file(READ "${eth_frames_bundle}/input_paths.json" eth_frames_inputs)
    string(JSON eth_frames_input_count LENGTH "${eth_frames_inputs}")
    math(EXPR eth_frames_last_input "${eth_frames_input_count} - 1")
    foreach(index RANGE 0 ${eth_frames_last_input})
        string(JSON eth_frames_input_name MEMBER "${eth_frames_inputs}" ${index})
        string(JSON eth_frames_input_path GET "${eth_frames_inputs}" "${eth_frames_input_name}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${eth_frames_input_path}")
    endforeach()
    set(eth_frames_target
        "nexus_${eth_frames_family}_eth_frames_factory_test")
    nexus_google_test(${eth_frames_target}
        SOURCES ethernet_frames_factory_test.cpp "${eth_frames_bundle}/bindings.c"
            "${NEXUS_SOURCE_DIR}/soc/${eth_frames_soc}/system.c"
            "${NEXUS_SOURCE_DIR}/soc/${eth_frames_soc}/drivers/gpio.c"
            "${NEXUS_SOURCE_DIR}/soc/${eth_frames_soc}/drivers/ethernet_frames.c"
            "${NEXUS_SOURCE_DIR}/io/src/eth_frames_mac.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${eth_frames_bundle}" "${NEXUS_SOURCE_DIR}/io/private"
            "${NEXUS_SOURCE_DIR}/soc/${eth_frames_soc}"
            "${NEXUS_SOURCE_DIR}/soc/${eth_frames_soc}/private")
    target_compile_options(${eth_frames_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/ethernet_frames_factory_model.h"
        -fdata-sections -ffunction-sections -fno-pie)
    target_compile_definitions(${eth_frames_target} PRIVATE
        _init=nx_eth_frames_fixture_init _fini=nx_eth_frames_fixture_fini)
    target_link_options(${eth_frames_target} PRIVATE -no-pie
        "-Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/eth_frames_host_dma.ld")
    set_property(TARGET ${eth_frames_target} APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/eth_frames_host_dma.ld")
    if(eth_frames_family STREQUAL "stm32")
        # Model excludes reset-only CPU assembly, never the actual clock/timer
        # transaction. CPP binds the real System context to the same MMIO map.
        target_compile_definitions(${eth_frames_target} PRIVATE
            NX_ETH_FRAMES_FACTORY_STM32 NEXUS_STM32_MODEL STM32F407xx
            USE_RTOS=0 USE_HAL_ETH_REGISTER_CALLBACKS=0)
        target_include_directories(${eth_frames_target} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
            "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
        nexus_stm32f4_official(${eth_frames_target})
    else()
        set(eth_frames_gd
            "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
        set(eth_frames_sdk "${eth_frames_target}_sdk")
        add_library(${eth_frames_sdk} STATIC
            "${eth_frames_gd}/GD32F4xx_standard_peripheral/Source/gd32f4xx_rcu.c"
            "${eth_frames_gd}/GD32F4xx_standard_peripheral/Source/gd32f4xx_fmc.c"
            "${eth_frames_gd}/GD32F4xx_standard_peripheral/Source/gd32f4xx_timer.c"
            "${eth_frames_gd}/GD32F4xx_standard_peripheral/Source/gd32f4xx_gpio.c")
        target_link_libraries(${eth_frames_sdk} PRIVATE nexus_build_options)
        target_include_directories(${eth_frames_sdk} PRIVATE
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model"
            "${NEXUS_SOURCE_DIR}/soc/gd32f470/private")
        target_include_directories(${eth_frames_sdk} SYSTEM PRIVATE
            "${eth_frames_gd}/CMSIS"
            "${eth_frames_gd}/CMSIS/GD/GD32F4xx/Include"
            "${eth_frames_gd}/GD32F4xx_standard_peripheral/Include")
        target_compile_definitions(${eth_frames_sdk} PRIVATE
            GD32F470 HXTAL_VALUE=25000000)
        target_compile_options(${eth_frames_sdk} PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
        target_sources(${eth_frames_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/gd32f470/timebase.c")
        target_link_libraries(${eth_frames_target} PRIVATE ${eth_frames_sdk})
        target_compile_definitions(${eth_frames_target} PRIVATE
            NX_ETH_FRAMES_FACTORY_GD32 NX_GD32_CLOCK_MODEL
            GD32F470 HXTAL_VALUE=25000000)
        target_include_directories(${eth_frames_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
        target_include_directories(${eth_frames_target} SYSTEM PRIVATE
            "${eth_frames_gd}/CMSIS"
            "${eth_frames_gd}/CMSIS/GD/GD32F4xx/Include"
            "${eth_frames_gd}/GD32F4xx_standard_peripheral/Include")
        target_compile_options(${eth_frames_target} PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
    endif()
endforeach()
