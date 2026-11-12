# SAI owner behavior once; master and alternate kernel retain distinct cases.
# Author: Nexus Team
nexus_google_test(nexus_audio_layout_test
    SOURCES audio_layout_test.cpp "${NEXUS_SOURCE_DIR}/io/src/api/audio.c"
    LIBRARIES Nexus::IOAudio Nexus::IOStream)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    foreach(slice slave master fraction)
        set(sai_test "nexus_stm32_sai_${slice}_test")
        set(sai_model "${CMAKE_CURRENT_SOURCE_DIR}/stm32_sai_model.h")
        set(sai_profile
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private/sai_hal")
        set(sai_definitions STM32F446xx NEXUS_STM32_MODEL=1)
        if(NOT slice STREQUAL "slave")
            list(APPEND sai_definitions NX_SAI_MASTER_TEST=1)
        endif()
        if(slice STREQUAL "fraction")
            list(APPEND sai_definitions NX_SAI_ALT_CLOCK=1)
        endif()
        nexus_google_test(${sai_test} STM32F4_LL
            SOURCES stm32_sai_test.cpp
                "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/audio_sai.c"
                "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/av.c"
            LIBRARIES Nexus::IOAudio Nexus::IOStream Nexus::Arch
            INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
                "${sai_profile}" "${CMAKE_CURRENT_SOURCE_DIR}")
        set(sai_hal "${sai_test}_hal")
        add_library(${sai_hal} OBJECT
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Source/Templates/system_stm32f4xx.c")
        foreach(module sai sai_ex dma rcc rcc_ex)
            target_sources(${sai_hal} PRIVATE
                "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Src/stm32f4xx_hal_${module}.c")
        endforeach()
        nexus_stm32f4_official(${sai_hal})
        target_include_directories(${sai_hal} PRIVATE
            "${sai_profile}" "${CMAKE_CURRENT_SOURCE_DIR}"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
            "${NEXUS_SOURCE_DIR}/arch/include")
        target_compile_features(${sai_hal} PRIVATE c_std_11)
        set_target_properties(${sai_hal} PROPERTIES C_EXTENSIONS OFF)
        target_link_libraries(${sai_hal} PRIVATE nexus_build_options
            nexus_io_contract)
        nexus_use_cmsis_host_model(${sai_hal})
        foreach(target ${sai_test} ${sai_hal})
            target_include_directories(${target} SYSTEM PRIVATE
                "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
            nexus_use_cmsis_core(${target})
            target_compile_definitions(${target} PRIVATE ${sai_definitions})
            target_compile_options(${target} PRIVATE
                "-include${sai_model}" -ffunction-sections -fdata-sections)
        endforeach()
        target_link_options(${sai_test} PRIVATE -Wl,--gc-sections)
        target_sources(${sai_test} PRIVATE $<TARGET_OBJECTS:${sai_hal}>)
    endforeach()
endif()
