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
    'sphinxcontrib.mermaid',
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

# Supported languages
supported_languages = {
    'en': 'English',
    'zh_CN': '中文 (简体)',
}

# Language-specific settings
if language == 'zh_CN':
    # Chinese-specific search configuration
    html_search_language = 'zh'
    # Enhanced Chinese search with word segmentation
    html_search_options = {
        'type': 'default',
        'lang': 'zh',
    }
else:
    html_search_language = 'en'
    # Enhanced English search
    html_search_options = {
        'type': 'default',
        'lang': 'en',
    }

# -- Options for HTML output -------------------------------------------------

html_theme = 'alabaster'
html_static_path = ['_static']
html_logo = None
html_favicon = None

# Custom CSS
html_css_files = [
    'custom.css',
]

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
    # Navigation options
    'show_related': True,
    'show_relbars': True,
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

# Additional sidebar templates
# These are included in the page content, not the sidebar
html_additional_pages = {}

# Show relations bar (previous/next) at top and bottom of page
html_show_sphinx = True

# Add language switcher context
html_context = {
    'languages': [
        ('en', 'English'),
        ('zh_CN', '中文'),
    ],
    'current_language': language,
}

# Enhanced search configuration
html_search_options = {
    'type': 'default',
}

# Show source link
html_show_sourcelink = True

# Show copyright in footer
html_show_copyright = True

# Show Sphinx info in footer
html_show_sphinx = True

# Add "Edit on GitHub" links
html_context.update({
    'display_github': True,
    'github_user': 'nexus-team',
    'github_repo': 'nexus',
    'github_version': 'main',
    'conf_py_path': '/docs/sphinx/',
})

# -- Breathe configuration ---------------------------------------------------
# Note: Doxygen must be run first from project root: doxygen Doxyfile
# This generates XML output to docs/api/xml

breathe_projects = {
    'nexus': '../../docs/api/xml'
}
breathe_default_project = 'nexus'
breathe_default_members = ('members', 'undoc-members')

# Suppress breathe warnings if Doxygen XML not found
# Also suppress duplicate C++ declaration warnings from forward declarations
suppress_warnings = [
    'breathe.doxygen',
    'cpp.duplicate_declaration',
    'ref.cpp'
]

# -- Mermaid configuration ---------------------------------------------------
# Mermaid is used for creating diagrams from text descriptions
# Supported diagram types: flowchart, sequence, class, state, ER, etc.

mermaid_version = "10.6.1"  # Use specific version for consistency
mermaid_init_js = """
mermaid.initialize({
    startOnLoad: true,
    theme: 'default',
    themeVariables: {
        primaryColor: '#667eea',
        primaryTextColor: '#fff',
        primaryBorderColor: '#5568d3',
        lineColor: '#667eea',
        secondaryColor: '#764ba2',
        tertiaryColor: '#f093fb'
    },
    flowchart: {
        useMaxWidth: true,
        htmlLabels: true,
        curve: 'basis'
    },
    sequence: {
        useMaxWidth: true,
        mirrorActors: true
    }
});
"""

# -- Graphviz configuration --------------------------------------------------
# Graphviz is used for generating graphs and diagrams
# Note: Graphviz must be installed on the system

graphviz_output_format = 'svg'  # Use SVG for better quality
graphviz_dot_args = [
    '-Gfontname=Arial',
    '-Nfontname=Arial',
    '-Efontname=Arial',
    '-Gfontsize=10',
    '-Nfontsize=10',
    '-Efontsize=10',
]

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

# -- Index generation --------------------------------------------------------

# Generate index for configuration options
html_domain_indices = True

# Generate module index
html_use_modindex = True

# Generate index
html_use_index = True

# Split index by letter
html_split_index = False

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
