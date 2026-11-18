# Selected audio sources and private dependencies.
nexus_controller_modes(audio_modes audio)
if(audio_modes)
    set(stm32_sai_hal OFF)
    nexus_stm32f4_gpio(nexus_soc_stm32f4)
    target_sources(
        nexus_soc_stm32f4
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../ip/av/av.c"
    )
    nexus_controller_names(audio_controllers audio)
    foreach(controller IN LISTS audio_controllers)
        nexus_controller_modes(audio_controller_modes audio "${controller}")
        if(controller MATCHES "^SAI[12]$")
            target_sources(
                nexus_soc_stm32f4
                PRIVATE "${CMAKE_CURRENT_LIST_DIR}/audio_sai.c"
            )
            if("${audio_controller_modes}" MATCHES "-blocks")
                target_sources(
                    nexus_soc_stm32f4
                    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/audio_sai_blocks.c"
                )
            endif()
            set(stm32_sai_hal ON)
        elseif(controller MATCHES "^I2S[23]$")
            target_sources(
                nexus_soc_stm32f4
                PRIVATE "${CMAKE_CURRENT_LIST_DIR}/audio.c"
            )
            if("${audio_controller_modes}" MATCHES "-blocks")
                target_sources(
                    nexus_soc_stm32f4
                    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/audio_blocks.c"
                )
            endif()
        else()
            message(FATAL_ERROR "Unreviewed audio controller: ${controller}")
        endif()
    endforeach()
    if(stm32_sai_hal)
        nexus_stm32f4_sai_hal(nexus_soc_stm32f4)
    endif()
endif()
