#-----------------------------------------------------------------------------
# Nexus Example Applications
#-----------------------------------------------------------------------------
# This file manages the build of example applications using a table-driven
# approach. Applications are conditionally built based on Kconfig settings.
#
# Configuration options are defined in applications/Kconfig
#
# To add a new application:
#   1. Add Kconfig option in applications/Kconfig (CONFIG_BUILD_APP_XXX)
#   2. Add entry to NEXUS_APPLICATIONS list below
#   3. Create application directory with CMakeLists.txt
#-----------------------------------------------------------------------------

# Only build embedded examples for non-native platforms
if(NOT NEXUS_PLATFORM STREQUAL "native")

    #-------------------------------------------------------------------------
    # Application Registry (Table-Driven)
    #-------------------------------------------------------------------------
    # Each application is defined by three properties:
    #   - Directory name
    #   - Kconfig option (CONFIG_BUILD_APP_XXX)
    #   - Description
    #
    # To add a new application:
    #   1. Add CONFIG_BUILD_APP_XXX option in applications/Kconfig
    #   2. Add three lines below (directory, config, description)
    #   3. Create application directory with CMakeLists.txt
    #-------------------------------------------------------------------------

    # Application directories
    set(APP_DIRS
        blinky
        shell_demo
        config_demo
        stm32_blinky
        freertos_demo
    )

    # Corresponding Kconfig options
    set(APP_CONFIGS
        CONFIG_BUILD_APP_BLINKY
        CONFIG_BUILD_APP_SHELL_DEMO
        CONFIG_BUILD_APP_CONFIG_DEMO
        CONFIG_BUILD_APP_STM32_BLINKY
        CONFIG_BUILD_APP_FREERTOS_DEMO
    )

    # Application descriptions
    set(APP_DESCS
        "Basic LED blinky example"
        "Interactive shell demo"
        "Configuration system demo"
        "STM32 minimal blinky"
        "FreeRTOS OSAL demo"
    )

    #-------------------------------------------------------------------------
    # Process Application Registry
    #-------------------------------------------------------------------------
    message(STATUS "Configuring example applications...")

    set(APPS_BUILT 0)
    set(APPS_SKIPPED 0)

    list(LENGTH APP_DIRS app_count)
    math(EXPR app_count_max "${app_count} - 1")

    foreach(idx RANGE ${app_count_max})
        # Get application properties
        list(GET APP_DIRS ${idx} app_dir)
        list(GET APP_CONFIGS ${idx} app_config)
        list(GET APP_DESCS ${idx} app_desc)

        # Check if application should be built
        set(should_build FALSE)
        if(DEFINED ${app_config} AND ${app_config})
            if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${app_dir})
                set(should_build TRUE)
            else()
                message(WARNING "Application '${app_dir}' enabled but directory not found")
            endif()
        endif()

        # Build or skip application
        if(should_build)
            add_subdirectory(${app_dir})
            message(STATUS "  [+] ${app_dir} - ${app_desc}")
            math(EXPR APPS_BUILT "${APPS_BUILT} + 1")
        else()
            message(STATUS "  [-] ${app_dir} - ${app_desc} (disabled)")
            math(EXPR APPS_SKIPPED "${APPS_SKIPPED} + 1")
        endif()
    endforeach()

    #-------------------------------------------------------------------------
    # Summary
    #-------------------------------------------------------------------------
    message(STATUS "Applications: ${APPS_BUILT} built, ${APPS_SKIPPED} skipped")

else()
    message(STATUS "Native platform: Skipping embedded applications")
endif()

#-----------------------------------------------------------------------------
# End of Example Applications
#-----------------------------------------------------------------------------
