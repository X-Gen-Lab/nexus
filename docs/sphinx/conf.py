"""Current public platform documentation; historical APIs are not built."""
from pathlib import Path

project = "Nexus Embedded Platform"
author = "Nexus Team"
copyright = "2026, Nexus Team"
version = release = "1.0.0"
extensions = ["breathe"]
language = "en"
exclude_patterns = ["_build", "README.md"]
html_theme = "alabaster"
html_theme_options = {"description": "Static embedded platform mechanisms"}
html_static_path = []
html_sidebars = {"**": ["about.html", "navigation.html", "searchbox.html"]}
breathe_projects = {"nexus": str(Path(__file__).resolve().parents[1] / "api/xml")}
breathe_default_project = "nexus"
breathe_domain_by_extension = {"h": "c"}
nitpicky = True

# External C/POSIX/FreeRTOS types and deliberate opaque provider tags have no
# definitions in the public-only Doxygen input. Keep other references strict.
_external_types = (
    "size_t uint8_t uint16_t uint32_t uint64_t int16_t int32_t int64_t uintptr_t "
    "pthread_t pthread_mutex_t pthread_cond_t "
    "QueueHandle_t SemaphoreHandle_t StackType_t StaticQueue_t TaskFunction_t "
    "StaticSemaphore_t StaticTask_t TaskHandle_t TickType_t UBaseType_t "
    "nx_adc_port nx_exti_port nx_flash_port nx_gpio_port nx_i2c_endpoint "
    "nx_i2c_port nx_pwm_port nx_spi_endpoint nx_spi_port nx_uart_port "
    "nx_watchdog_port"
).split()
nitpick_ignore = [("c:identifier", name) for name in _external_types]
