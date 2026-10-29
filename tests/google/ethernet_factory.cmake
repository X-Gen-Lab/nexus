foreach(eth_family stm32 gd32)
    if(eth_family STREQUAL "stm32")
        set(eth_label "stm32-zg")
        set(eth_soc "stm32f407")
    else()
        set(eth_label "gd32")
        set(eth_soc "gd32f470")
    endif()
    set(eth_input "${NEXUS_SOURCE_DIR}/tests/contracts/ethernet_assembly/${eth_label}.toml")
    set(eth_bundle "${CMAKE_CURRENT_BINARY_DIR}/eth-factory-${eth_family}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${eth_input}"
        --output "${eth_bundle}" RESULT_VARIABLE eth_generated
        OUTPUT_VARIABLE eth_output ERROR_VARIABLE eth_error)
    if(NOT eth_generated EQUAL 0)
        message(FATAL_ERROR "Ethernet factory fixture rejected: ${eth_output}${eth_error}")
    endif()
    nexus_google_configuration_inputs("${eth_bundle}")
    set(eth_target "nexus_${eth_family}_eth_factory_test")
    nexus_google_test(${eth_target} SOURCES ethernet_factory_test.cpp
        "${eth_bundle}/bindings.c"
        "${NEXUS_SOC_SOURCE_${eth_soc}}/drivers/ethernet.c"
        "${NEXUS_SOURCE_DIR}/io/src/controllers/eth_mac.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${eth_bundle}" "${NEXUS_SOURCE_DIR}/io/private"
            "${NEXUS_SOURCE_DIR}/soc/native/drivers"
            "${NEXUS_SOC_SOURCE_${eth_soc}}/private")
    target_compile_options(${eth_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/ethernet_model.h"
        "-include${CMAKE_CURRENT_SOURCE_DIR}/ethernet_factory_model.h")
    target_compile_definitions(${eth_target} PRIVATE
        _init=nx_eth_fixture_init _fini=nx_eth_fixture_fini)
    if(eth_family STREQUAL "stm32")
        target_sources(${eth_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/drivers/platform_cold.c")
        target_include_directories(${eth_target} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
            "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
        nexus_use_cmsis_core(${eth_target})
        target_compile_definitions(${eth_target} PRIVATE STM32F407xx NX_ETH_FACTORY_STM32)
    else()
        target_sources(${eth_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/drivers/platform_cold.c")
        set(eth_gd "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
        target_include_directories(${eth_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
        target_include_directories(${eth_target} SYSTEM PRIVATE
            "${eth_gd}/CMSIS/GD/GD32F4xx/Include"
            "${eth_gd}/GD32F4xx_standard_peripheral/Include")
        target_compile_definitions(${eth_target} PRIVATE GD32F470 HXTAL_VALUE=25000000 NX_ETH_FACTORY_GD32)
        target_compile_options(${eth_target} PRIVATE
            "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
    endif()
endforeach()
