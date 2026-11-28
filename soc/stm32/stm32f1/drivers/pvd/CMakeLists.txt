# Compile the internal comparator owner only for the selected polling mode.
# Author: Nexus Team
nexus_controller_modes(f1_pvd exti)
if("pvd-level" IN_LIST f1_pvd)
    target_sources(nexus_soc_stm32f1 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/pvd.c")
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOEXTI)
endif()
