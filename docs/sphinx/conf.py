"""Current task-oriented site; source root is docs/, not this directory."""
import os
import sys
from pathlib import Path

_REPOSITORY = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(_REPOSITORY))

project = 'Nexus Embedded Platform'
author = 'Nexus Team'
copyright = '2026, Nexus Team'
version = release = os.environ.get('NEXUS_DOCS_VERSION', 'source-preview')
extensions = ['myst_parser', 'breathe', 'sphinx.ext.graphviz',
              'tools.documentation.sphinx_ext']
source_suffix = {'.md': 'markdown', '.rst': 'restructuredtext'}
root_doc = 'index'
language = os.environ.get('NEXUS_DOCS_LANGUAGE', 'zh_CN')
exclude_patterns = ['archive/**', 'api/**', '_build/**', '_output/**',
                    'locale/**', 'README.md', 'sphinx/README.md',
                    'sphinx/index.rst', 'sphinx/api.rst',
                    'sphinx/getting_started.rst', 'sphinx/architecture.rst',
                    'sphinx/configuration.rst', 'sphinx/migration.rst',
                    'sphinx/os.rst', 'sphinx/support.rst',
                    'sphinx/development/index.rst']
html_theme = 'alabaster'
html_theme_options = {'description': ('可复用的静态嵌入式平台机制'
                      if language == 'zh_CN' else 'Static embedded platform mechanisms')}
templates_path = [str(_REPOSITORY / 'docs/sphinx/templates')]
html_static_path = [str(_REPOSITORY / 'docs/assets/styles')]
html_css_files = ['nexus.css']
html_sidebars = {'**': ['about.html', 'navigation.html', 'searchbox.html']}
html_show_sourcelink = False
html_copy_source = False
html_domain_indices = True
myst_heading_anchors = 5
myst_enable_extensions = ['colon_fence']
gettext_compact = False
gettext_uuid = False
gettext_location = True
locale_dirs = ['locale']
graphviz_output_format = 'svg'
breathe_projects = {'nexus': os.environ.get('NEXUS_DOCS_DOXYGEN_XML',
                    str(_REPOSITORY / 'docs/api/xml'))}
breathe_default_project = 'nexus'
breathe_domain_by_extension = {'h': 'c'}
nitpicky = True

# These deliberately opaque provider tags and actual external kernel/C types
# are not defined by the public-only Doxygen input. Other references stay strict.
_external_types = (
    'size_t uint8_t uint16_t uint32_t uint64_t int16_t int32_t int64_t uintptr_t '
    'pthread_t pthread_mutex_t pthread_cond_t '
    'QueueHandle_t SemaphoreHandle_t StackType_t StaticQueue_t TaskFunction_t '
    'StaticSemaphore_t StaticTask_t TaskHandle_t TickType_t UBaseType_t '
    'TX_THREAD TX_SEMAPHORE UINT nx_clock_domain nx_user_io_broker nx_user_io_call '
    'nx_adc_port nx_exti_port nx_flash_port nx_gpio_port nx_i2c_endpoint '
    'nx_i2c_port nx_pwm_port nx_spi_endpoint nx_spi_port nx_uart_port '
    'nx_watchdog_port'
).split()
nitpick_ignore = [('c:identifier', name) for name in _external_types]
