# Select the fixed F103 I2C1 polling implementation.
# Author: Nexus Team
nexus_controller_modes(i2c_modes i2c)
if("poll-messages" IN_LIST i2c_modes)
    target_sources(nexus_soc_stm32f1 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/i2c.c")
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOI2C)
endif()
