# Compile the TIM3 owner only for a selected encoder.
# Author: Nexus Team
if("quadrature" IN_LIST f1_encoder)
    target_sources(
        nexus_soc_stm32f1
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/encoder.c"
    )
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOEncoder)
endif()
