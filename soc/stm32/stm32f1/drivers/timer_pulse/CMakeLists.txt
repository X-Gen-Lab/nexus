# Build only the selected TIM3 one-pulse owner.
# Author: Nexus Team
if("one-pulse" IN_LIST f1_timer_pulse)
    target_sources(nexus_soc_stm32f1 PRIVATE tim3.c)
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOTimerPulse)
endif()
