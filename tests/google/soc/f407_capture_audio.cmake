# Real C11 F407 owners; vendor cold init and MMIO leaves remain independent.
# Author: Nexus Team
nexus_google_test(nexus_f407_capture_audio_test STM32F4_LL
    SOURCES "${nexus_google_root}/soc/f407_capture_audio_test.cpp"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ip/av/av.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/capture/capture.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/audio/audio.c"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/audio/audio_blocks.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/sdk/hal_config/capture"
        "${nexus_google_root}/soc")
target_include_directories(nexus_f407_capture_audio_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_f407_capture_audio_test)
target_compile_definitions(nexus_f407_capture_audio_test PRIVATE STM32F407xx
    NEXUS_STM32_MODEL=1 NX_STM32_CAPTURE_MAX_BITS=14U)
target_compile_options(nexus_f407_capture_audio_test PRIVATE
    "-include${nexus_google_root}/soc/f407_capture_audio_model.h"
    -ffunction-sections -fdata-sections)
target_link_options(nexus_f407_capture_audio_test PRIVATE -Wl,--gc-sections)
add_library(nexus_capture_test_hal OBJECT
    "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Src/stm32f4xx_hal_dcmi.c")
nexus_stm32f4_official(nexus_capture_test_hal)
target_include_directories(nexus_capture_test_hal PRIVATE
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/sdk/hal_config/capture")
target_include_directories(nexus_capture_test_hal SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
nexus_use_cmsis_core(nexus_capture_test_hal)
nexus_use_cmsis_host_model(nexus_capture_test_hal)
target_compile_definitions(nexus_capture_test_hal PRIVATE STM32F407xx)
target_compile_features(nexus_capture_test_hal PRIVATE c_std_11)
target_compile_options(nexus_capture_test_hal PRIVATE
    -ffunction-sections -fdata-sections)
target_sources(nexus_f407_capture_audio_test PRIVATE
    $<TARGET_OBJECTS:nexus_capture_test_hal>)
