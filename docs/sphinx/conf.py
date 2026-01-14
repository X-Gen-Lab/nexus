"""
Sphinx Configuration for Nexus Embedded Platform Documentation
Nexus 嵌入式平台文档 Sphinx 配置

Supports internationalization (i18n) using Sphinx's official gettext mechanism.
支持使用 Sphinx 官方 gettext 机制的国际化。
"""

import os
import sys
from pathlib import Path

# -- Project information -----------------------------------------------------

project = 'Nexus Embedded Platform'
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
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store', 'locale', '*.md']

# -- Internationalization (i18n) ---------------------------------------------
# Using Sphinx's official gettext-based i18n mechanism
# https://www.sphinx-doc.org/en/master/usage/advanced/intl.html

# Default language (source language)
language = 'en'

# Directory for translation files (.po/.mo)
locale_dirs = ['locale/']

# Create separate .pot files for each document (recommended for large projects)
gettext_compact = False

# Include UUID for each message (helps track changes)
gettext_uuid = True

# Include source location in .pot files
gettext_location = True

# Additional targets for translation extraction
gettext_additional_targets = ['literal-block', 'doctest-block', 'raw', 'image']

# -- Options for HTML output -------------------------------------------------

html_theme = 'alabaster'
html_static_path = ['_static']
html_logo = None
html_favicon = None

# Language-specific theme options
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
        'language_switcher.html',
        'about.html',
        'navigation.html',
        'relations.html',
        'searchbox.html',
    ]
}

# Add language switcher context
html_context = {
    'languages': [
        ('en', 'English'),
        ('zh_CN', '中文'),
    ],
    'current_language': language,
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

source_suffix = '.rst'

# Note: To enable Markdown support, install myst-parser:
#   pip install myst-parser
# Then uncomment below and add 'myst_parser' to extensions:
# source_suffix = {
#     '.rst': 'restructuredtext',
#     '.md': 'markdown',
# }

# -- Master document ---------------------------------------------------------

master_doc = 'index'

# -- Intersphinx configuration -----------------------------------------------

intersphinx_mapping = {
    'python': ('https://docs.python.org/3', None),
}

# -- Language-specific overrides ---------------------------------------------

def setup(app):
    """Setup hook for language-specific customizations."""
    # Update current_language in html_context based on actual build language
    app.config.html_context['current_language'] = app.config.language

    # Override theme description for Chinese
    if app.config.language == 'zh_CN':
        app.config.html_theme_options['description'] = '世界级嵌入式软件开发平台'
        app.config.project = 'Nexus 嵌入式平台'
