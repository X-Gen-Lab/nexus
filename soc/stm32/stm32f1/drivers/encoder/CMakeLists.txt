# Compile the TIM3 owner only for a selected encoder.
# Author: Nexus Team
if("quadrature" IN_LIST f1_encoder)
    target_sources(
        nexus_soc_stm32f1
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/encoder.c"
    )
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOEncoder)

    # The exact TIM3 assembly owns one counting mode. Keep it constant in C.
    file(READ "${NEXUS_CONFIG_DIR}/resolved.json" encoder_resolved)
    string(JSON encoder_count LENGTH "${encoder_resolved}" controllers)
    math(EXPR encoder_last "${encoder_count} - 1")
    foreach(index RANGE "${encoder_last}")
        string(JSON kind GET "${encoder_resolved}" controllers "${index}" kind)
        if(kind STREQUAL "encoder")
            string(
                JSON counting
                GET "${encoder_resolved}"
                controllers
                "${index}"
                counting
            )
            string(REPLACE "-" "_" suffix "${counting}")
            string(TOUPPER "${suffix}" suffix)
            target_compile_definitions(
                nexus_soc_stm32f1
                PRIVATE "NX_STM32F1_ENCODER_MODE=NX_ENCODER_MODE_${suffix}"
            )
            break()
        endif()
    endforeach()
endif()
