# Nexus Documentation

Welcome to the Nexus Embedded Platform documentation source!

## 📚 Documentation Structure

```
docs/sphinx/
├── index.rst                    # Main documentation entry point
├── DOCUMENTATION_GUIDE.rst      # Navigation and usage guide
├── conf.py                      # Sphinx configuration
├── build_docs.py               # Build script with i18n support
├── translate_helper.py         # Translation automation tool
│
├── getting_started/            # Getting started guides
├── user_guide/                 # User documentation
├── tutorials/                  # Step-by-step tutorials
├── platform_guides/            # Platform-specific guides
├── api/                        # API reference
├── reference/                  # Reference documentation
├── development/                # Development and contributing
│
├── _static/                    # Static files (CSS, images)
│   └── custom.css             # Custom styling
├── _templates/                 # Sphinx templates
│   ├── language_switcher.html # Language selector
│   ├── module_template.rst    # Module documentation template
│   └── platform_guide_template.rst
│
├── locale/                     # Translations
│   ├── zh_CN/                 # Chinese (Simplified)
│   │   └── LC_MESSAGES/       # Translation files (.po)
│   └── README_zh.md           # Translation guide
│
└── _build/                     # Build output (generated)
    └── html/
        ├── en/                # English documentation
        └── zh_CN/             # Chinese documentation
```

## 🚀 Quick Start

### Build Documentation

```bash
# Build all languages
python build_docs.py

# Build specific language
python build_docs.py --lang en
python build_docs.py --lang zh_CN

# Clean and rebuild
python build_docs.py --clean

# Build and serve locally
python build_docs.py --serve
```

### View Documentation

After building, open in browser:
- English: `_build/html/en/index.html`
- Chinese: `_build/html/zh_CN/index.html`
- Language selector: `_build/html/index.html`

Or serve locally:
```bash
python build_docs.py --serve
# Visit http://localhost:8000
```

## 🌍 Internationalization (i18n)

### Supported Languages

- **English (en)** - Default/source language
- **Chinese Simplified (zh_CN)** - 中文（简体）

### Translation Workflow

#### 1. Extract translatable strings

```bash
python build_docs.py --update-po
```

This extracts all translatable text from `.rst` files into `.pot` and `.po` files.

#### 2. Auto-translate common terms

```bash
python translate_helper.py zh_CN --auto-translate
```

Automatically translates technical terms and common phrases.

#### 3. Manual translation

Edit `.po` files in `locale/zh_CN/LC_MESSAGES/`:

```bash
# Use any text editor
notepad locale/zh_CN/LC_MESSAGES/user_guide/hal.po

# Or use Poedit (recommended)
poedit locale/zh_CN/LC_MESSAGES/user_guide/hal.po
```

#### 4. Build translated documentation

```bash
python build_docs.py --lang zh_CN
```

#### 5. Validate translations

```bash
python translate_helper.py zh_CN --validate
```

### Translation Statistics

```bash
python translate_helper.py zh_CN --stats
```

### Adding New Language

```bash
# Initialize new language (e.g., Japanese)
python build_docs.py --init-po ja

# This creates locale/ja/LC_MESSAGES/ with .po files
```

## 📝 Writing Documentation

### File Format

Documentation uses reStructuredText (`.rst`) format:

```rst
Section Title
=============

Subsection
----------

**Bold text** and *italic text*

Code example:

.. code-block:: c

   int main(void) {
       return 0;
   }

Cross-reference: :doc:`other_page`
```

### Using Templates

Copy templates from `_templates/`:

```bash
# Module documentation
cp _templates/module_template.rst user_guide/my_module.rst

# Platform guide
cp _templates/platform_guide_template.rst platform_guides/my_platform.rst

# Tutorial
cp _templates/tutorial_template.rst tutorials/my_tutorial.rst
```

### Documentation Conventions

**Code blocks:**
```rst
.. code-block:: c

   /* C code */
   
.. code-block:: bash

   # Shell commands
   
.. code-block:: kconfig

   # Kconfig options
```

**Admonitions:**
```rst
.. note::

   Important information

.. warning::

   Critical warning

.. tip::

   Helpful suggestion
```

**Cross-references:**
```rst
:doc:`path/to/document`      # Link to document
:ref:`label-name`            # Link to label
```

**External links:**
```rst
`Link text <https://example.com>`_
```

## 🔧 Advanced Features

### Doxygen Integration

Generate API documentation from C code:

```bash
# From project root
doxygen Doxyfile

# Then build Sphinx docs
cd docs/sphinx
python build_docs.py
```

### Mermaid Diagrams

Create diagrams using Mermaid syntax:

```rst
.. mermaid::

   graph LR
       A[Start] --> B[Process]
       B --> C[End]
```

### Custom Styling

Edit `_static/custom.css` to customize appearance.

### Search Configuration

Search is automatically configured for each language with appropriate tokenization.

## 🛠️ Build Scripts

### build_docs.py

Main documentation build script with i18n support.

**Options:**
- `--lang LANG` - Build specific language
- `--clean` - Clean before building
- `--serve` - Serve documentation locally
- `--port PORT` - Server port (default: 8000)
- `--doxygen` - Run Doxygen first
- `--update-po` - Update translation files
- `--init-po LANG` - Initialize new language

### translate_helper.py

Translation automation and validation tool.

**Options:**
- `--stats` - Show translation statistics
- `--auto-translate` - Auto-translate common terms
- `--validate` - Validate .po files
- `--all` - Run all operations
- `--dry-run` - Preview without changes

## 📋 Requirements

Install Python dependencies:

```bash
pip install -r requirements.txt
```

Required packages:
- `sphinx` - Documentation generator
- `sphinx-intl` - Internationalization support
- `breathe` - Doxygen integration
- `sphinxcontrib-mermaid` - Mermaid diagrams

Optional:
- `sphinx-rtd-theme` - Read the Docs theme
- `sphinx-autobuild` - Auto-rebuild on changes

## 🎨 Themes

Current theme: **Alabaster** (default Sphinx theme)

To change theme, edit `conf.py`:

```python
html_theme = 'sphinx_rtd_theme'  # Read the Docs theme
# or
html_theme = 'furo'              # Furo theme
```

## 📊 Documentation Quality

### Validation

```bash
# Check for broken links
sphinx-build -b linkcheck . _build/linkcheck

# Check for warnings
python build_docs.py 2>&1 | grep WARNING
```

### Coverage

```bash
# Check documentation coverage
sphinx-build -b coverage . _build/coverage
cat _build/coverage/python.txt
```

## 🤝 Contributing

See [development/documentation_contributing.rst](development/documentation_contributing.rst) for:

- Documentation style guide
- Writing guidelines
- Review process
- Translation guidelines

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/X-Gen-Lab/nexus/issues)
- **Discussions**: [GitHub Discussions](https://github.com/X-Gen-Lab/nexus/discussions)
- **Documentation Guide**: See `DOCUMENTATION_GUIDE.rst` in built docs

## 📄 License

Documentation is licensed under CC BY 4.0.

Code examples in documentation are licensed under the same license as Nexus (see project LICENSE file).

---

**Happy documenting! 📚✨**
