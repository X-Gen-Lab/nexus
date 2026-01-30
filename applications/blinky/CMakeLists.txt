##############################################################################
# Blinky Example Application
##############################################################################

cmake_minimum_required(VERSION 3.16)

#-----------------------------------------------------------------------------
# Application Configuration
#-----------------------------------------------------------------------------

# Application name
set(APP_NAME blinky)

# Application sources
set(APP_SOURCES
    main.c
)

#-----------------------------------------------------------------------------
# Create Executable
#-----------------------------------------------------------------------------

add_executable(${APP_NAME} ${APP_SOURCES})

# Include directories
target_include_directories(${APP_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(${APP_NAME}
    PRIVATE
        platform_stm32
        hal_interface
        hal
        osal_interface
        osal
)

# Compile definitions
target_compile_definitions(${APP_NAME}
    PRIVATE
        APP_NAME="${APP_NAME}"
        APP_VERSION="1.0.0"
)

#-----------------------------------------------------------------------------
# Post-Build Actions
#-----------------------------------------------------------------------------

# Generate binary and hex files
if(CMAKE_OBJCOPY)
    add_custom_command(TARGET ${APP_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${APP_NAME}> ${APP_NAME}.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${APP_NAME}> ${APP_NAME}.hex
        COMMENT "Generating ${APP_NAME}.bin and ${APP_NAME}.hex"
    )
endif()

# Print size information
if(CMAKE_SIZE)
    add_custom_command(TARGET ${APP_NAME} POST_BUILD
        COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:${APP_NAME}>
        COMMENT "Size of ${APP_NAME}:"
    )
endif()

message(STATUS "Configured application: ${APP_NAME}")

#-----------------------------------------------------------------------------
# End of Blinky Example Application
#-----------------------------------------------------------------------------
