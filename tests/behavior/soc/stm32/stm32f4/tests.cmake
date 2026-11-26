# One directory scope preserves each reviewed private model context.
# Author: Nexus Team
foreach(
    nexus_behavior_group
    pwm_adc
    dac
    system
    crc
    crypto
    graphics
    audio_video
    i2c
    uart
    adc
    memory
    rtc
    sdio
    spi
    shared
    usb
    can
    timer
    ethernet
    dma
    gpio
    watchdog
    entropy
)
    set(nexus_behavior_domain "soc;stm32f4;${nexus_behavior_group}")
    include("${CMAKE_CURRENT_LIST_DIR}/${nexus_behavior_group}/tests.cmake")
endforeach()
