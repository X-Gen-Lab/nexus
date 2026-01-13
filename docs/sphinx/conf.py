"""
Sphinx Configuration for Nexus Embedded Platform Documentation
Nexus 嵌入式平台文档 Sphinx 配置

Supports both English and Chinese documentation.
支持中英文双语文档。
"""

import os
import sys

# -- Project information -----------------------------------------------------

project = 'Nexus Embedded Platform'
project_cn = 'Nexus 嵌入式平台'
copyright = '2026, Nexus Team'
author = 'Nexus Team'
version = '1.0.0'
release = '1.0.0'

# -- General configuration ---------------------------------------------------

extensions = [
    'breathe',
    'sphinx.ext.autodoc',
    'sphinx.ext.viewcode',
    'sphinx.ext.todo',
    'sphinx.ext.graphviz',
    'sphinx.ext.intersphinx',
]

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store', 'index_cn.rst',
                    'getting_started/introduction_cn.rst',
                    'getting_started/installation_cn.rst',
                    'getting_started/quickstart_cn.rst',
                    'user_guide/architecture_cn.rst',
                    'user_guide/hal_cn.rst',
                    'user_guide/osal_cn.rst',
                    'user_guide/log_cn.rst',
                    'user_guide/porting_cn.rst',
                    'development/contributing_cn.rst',
                    'development/coding_standards_cn.rst',
                    'development/testing_cn.rst',
                    'conf_cn.py']

# -- Internationalization (i18n) ---------------------------------------------

# Supported languages
language = 'en'
locale_dirs = ['locale/']
gettext_compact = False

# -- Options for HTML output -------------------------------------------------

html_theme = 'alabaster'
html_static_path = ['_static']
html_logo = None
html_favicon = None

html_theme_options = {
    'logo_name': True,
    'description': 'World-class Embedded Software Development Platform',
    'github_user': 'nexus-team',
    'github_repo': 'nexus',
    'github_button': True,
    'github_type': 'star',
    'fixed_sidebar': True,
    'sidebar_collapse': True,
}

html_sidebars = {
    '**': [
        'about.html',
        'navigation.html',
        'relations.html',
        'searchbox.html',
    ]
}

# -- Breathe configuration ---------------------------------------------------
# Note: Doxygen must be run first from project root: doxygen Doxyfile
# This generates XML output to docs/api/xml

breathe_projects = {
    'nexus': '../../docs/api/xml'
}
breathe_default_project = 'nexus'
breathe_default_members = ('members', 'undoc-members')

# Suppress breathe warnings if Doxygen XML not found
suppress_warnings = ['breathe.doxygen']

# -- Todo extension configuration --------------------------------------------

todo_include_todos = True

# -- Source suffix -----------------------------------------------------------

source_suffix = {
    '.rst': 'restructuredtext',
    '.md': 'markdown',
}

# -- Master document ---------------------------------------------------------

master_doc = 'index'

# -- Intersphinx configuration -----------------------------------------------

intersphinx_mapping = {
    'python': ('https://docs.python.org/3', None),
}
