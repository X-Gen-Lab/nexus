# STM32F103ZE finite I2S2 master transmitter.
# Author: Nexus Team
if("master-tx" IN_LIST f1_audio)
    target_sources(nexus_soc_stm32f1 PRIVATE i2s2.c)
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOAudio)
endif()
