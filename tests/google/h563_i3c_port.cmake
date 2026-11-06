# Real maintained C11 port, official LL and a FIFO-only hardware model.
# Author: Nexus Team
set(h563_i3c_port_root
    "${NEXUS_SOURCE_DIR}/components/soc/stm32/stm32h563_i3c")
nexus_google_test(nexus_h563_i3c_port_test
    SOURCES h563_i3c_port_test.cpp
        "${h563_i3c_port_root}/src/stm32h563_i3c_port.c"
    LIBRARIES Nexus::I3CController
    INCLUDES "${h563_i3c_port_root}/private")
include("${NEXUS_SOURCE_DIR}/cmake/platform/STM32Official.cmake")
nexus_stm32_official(nexus_h563_i3c_port_test h5)
target_include_directories(nexus_h563_i3c_port_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h5/Include")
nexus_use_cmsis_core(nexus_h563_i3c_port_test)
target_compile_definitions(nexus_h563_i3c_port_test PRIVATE STM32H563xx)
target_compile_options(nexus_h563_i3c_port_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h563_i3c_port_model.h")
