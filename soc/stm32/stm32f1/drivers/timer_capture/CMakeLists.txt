# Compile the reviewed TIM4 capture owner only when selected.
# Author: Nexus Team
if("edge-capture" IN_LIST f1_timer_capture)
    target_sources(nexus_soc_stm32f1 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/tim4.c")
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOTimerCapture)
endif()
