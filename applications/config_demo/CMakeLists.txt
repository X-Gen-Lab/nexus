##############################################################################
# Config Demo Example Application
##############################################################################

cmake_minimum_required(VERSION 3.16)

# Application name
set(APP_NAME config_demo)

# Application sources
add_executable(${APP_NAME}
    main.c
)

# Link with platform, HAL, and Config framework
target_link_libraries(${APP_NAME}
    PRIVATE
        platform_stm32f4
        hal_interface
        hal
        config_manager
)

# Set linker script
if(DEFINED NEXUS_LINKER_SCRIPT)
    target_link_options(${APP_NAME}
        PRIVATE
            -T${NEXUS_LINKER_SCRIPT}
            -Wl,-Map=${APP_NAME}.map
            -Wl,--gc-sections
    )
endif()

# Generate binary and hex files
add_custom_command(TARGET ${APP_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${APP_NAME}> ${APP_NAME}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${APP_NAME}> ${APP_NAME}.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${APP_NAME}>
    COMMENT "Generating ${APP_NAME}.bin and ${APP_NAME}.hex"
)
