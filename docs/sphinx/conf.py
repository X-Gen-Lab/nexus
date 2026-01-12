"""
Sphinx Configuration for Nexus Embedded Platform Documentation

This configuration integrates Doxygen XML output via Breathe extension.
"""

import os
import sys

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
    'sphinx_rtd_theme',
]

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

# -- Options for HTML output -------------------------------------------------

html_theme = 'sphinx_rtd_theme'
html_static_path = ['_static']
html_logo = None
html_favicon = None

html_theme_options = {
    'logo_only': False,
    'display_version': True,
    'prev_next_buttons_location': 'bottom',
    'style_external_links': False,
    'collapse_navigation': True,
    'sticky_navigation': True,
    'navigation_depth': 4,
    'includehidden': True,
    'titles_only': False
}

# -- Breathe configuration ---------------------------------------------------

breathe_projects = {
    'nexus': '../api/xml'
}
breathe_default_project = 'nexus'
breathe_default_members = ('members', 'undoc-members')

# -- Todo extension configuration --------------------------------------------

todo_include_todos = True

# -- Language configuration --------------------------------------------------

language = 'en'

# -- Source suffix -----------------------------------------------------------

source_suffix = {
    '.rst': 'restructuredtext',
    '.md': 'markdown',
}

# -- Master document ---------------------------------------------------------

master_doc = 'index'
