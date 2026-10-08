nexus_add_application(TARGET stm32_config_test VERSION "1.0.0" SOURCES main.c config_display.c
    PLATFORM_DEPS Nexus::STM32SDK)
