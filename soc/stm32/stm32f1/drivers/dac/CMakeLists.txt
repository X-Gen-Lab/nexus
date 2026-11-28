# Select the single analog route proved by the generated assembly.
# Author: Nexus Team
if("direct" IN_LIST f1_dac)
    set(dac_source "${CMAKE_CURRENT_LIST_DIR}/dac.c")
    file(READ "${NEXUS_CONFIG_DIR}/resolved.json" f1_dac_resolved)
    string(JSON f1_dac_controller_count LENGTH "${f1_dac_resolved}" controllers)
    math(EXPR f1_dac_last_controller "${f1_dac_controller_count} - 1")
    foreach(index RANGE "${f1_dac_last_controller}")
        string(JSON kind GET "${f1_dac_resolved}" controllers "${index}" kind)
        if(kind STREQUAL "dac")
            string(
                JSON channel
                GET "${f1_dac_resolved}"
                controllers
                "${index}"
                channels
                0
            )
            set_property(
                SOURCE "${dac_source}" TARGET_DIRECTORY nexus_soc_stm32f1
                PROPERTY COMPILE_DEFINITIONS "NX_STM32F1_DAC_CHANNEL=${channel}"
            )
            break()
        endif()
    endforeach()
    target_sources(nexus_soc_stm32f1 PRIVATE "${dac_source}")
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IODAC)
endif()
