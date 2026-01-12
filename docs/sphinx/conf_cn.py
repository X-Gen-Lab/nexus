"""
Sphinx Configuration for Nexus Embedded Platform Documentation (Chinese)
Nexus 嵌入式平台文档 Sphinx 配置（中文版）
"""

import os
import sys

# -- Project information -----------------------------------------------------

project = 'Nexus 嵌入式平台'
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
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store', 'conf_cn.py',
                    'index.rst',
                    'getting_started/introduction.rst',
                    'getting_started/installation.rst',
                    'getting_started/quickstart.rst',
                    'user_guide/architecture.rst',
                    'user_guide/hal.rst',
                    'user_guide/osal.rst',
                    'user_guide/porting.rst',
                    'development/contributing.rst',
                    'development/coding_standards.rst',
                    'development/testing.rst']

# -- Internationalization (i18n) ---------------------------------------------

language = 'zh_CN'

# -- Options for HTML output -------------------------------------------------

html_theme = 'alabaster'
html_static_path = ['_static']
html_logo = None
html_favicon = None

html_theme_options = {
    'logo_name': True,
    'description': '世界级嵌入式软件开发平台',
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

breathe_projects = {
    'nexus': '../api/xml'
}
breathe_default_project = 'nexus'
breathe_default_members = ('members', 'undoc-members')

# -- Todo extension configuration --------------------------------------------

todo_include_todos = True

# -- Source suffix -----------------------------------------------------------

source_suffix = {
    '.rst': 'restructuredtext',
    '.md': 'markdown',
}

# -- Master document ---------------------------------------------------------

master_doc = 'index_cn'

# -- Intersphinx configuration -----------------------------------------------

intersphinx_mapping = {
    'python': ('https://docs.python.org/3', None),
}
