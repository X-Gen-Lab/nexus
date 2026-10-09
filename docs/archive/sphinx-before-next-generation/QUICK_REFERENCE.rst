Quick Reference Card
====================

Essential commands and references for Nexus documentation.

.. contents:: Quick Links
   :local:
   :depth: 1

Build Commands
--------------

**Build all languages:**

.. code-block:: bash

   python build_docs.py

**Build specific language:**

.. code-block:: bash

   python build_docs.py --lang en      # English
   python build_docs.py --lang zh_CN   # Chinese

**Clean and rebuild:**

.. code-block:: bash

   python build_docs.py --clean

**Build and serve:**

.. code-block:: bash

   python build_docs.py --serve
   # Visit http://localhost:8000

**Full build (with Doxygen):**

.. code-block:: bash

   python build_docs.py --doxygen
   # or
   make full

Translation Commands
--------------------

**Update translation files:**

.. code-block:: bash

   python build_docs.py --update-po

**Auto-translate common terms:**

.. code-block:: bash

   python translate_helper.py zh_CN --auto-translate

**Show translation statistics:**

.. code-block:: bash

   python translate_helper.py zh_CN --stats

**Validate translations:**

.. code-block:: bash

   python translate_helper.py zh_CN --validate

**Initialize new language:**

.. code-block:: bash

   python build_docs.py --init-po ja  # Japanese

Makefile Shortcuts
------------------

.. code-block:: bash

   make help          # Show all targets
   make html-all      # Build all languages
   make html-zh_CN    # Build Chinese only
   make serve         # Build and serve
   make full          # Clean + Doxygen + Build
   make stats         # Translation statistics
   make validate      # Validate translations
   make linkcheck     # Check for broken links

reStructuredText Syntax
------------------------

**Headings:**

.. code-block:: rst

   Main Title
   ==========

   Section
   -------

   Subsection
   ~~~~~~~~~~

**Text formatting:**

.. code-block:: rst

   **bold text**
   *italic text*
   ``inline code``

**Code blocks:**

.. code-block:: rst

   .. code-block:: c

      int main(void) {
          return 0;
      }

**Lists:**

.. code-block:: rst

   * Bullet item 1
   * Bullet item 2

   1. Numbered item 1
   2. Numbered item 2

**Links:**

.. code-block:: rst

   :doc:`path/to/document`           # Internal document
   :ref:`label-name`                 # Internal reference
   `External Link <https://...>`_    # External link

**Admonitions:**

.. code-block:: rst

   .. note::

      Important information

   .. warning::

      Critical warning

   .. tip::

      Helpful suggestion

**Tables:**

.. code-block:: rst

   +--------+--------+
   | Header | Header |
   +========+========+
   | Cell   | Cell   |
   +--------+--------+

**Images:**

.. code-block:: rst

   .. image:: path/to/image.png
      :alt: Alternative text
      :width: 400px

**Mermaid diagrams:**

.. code-block:: rst

   .. mermaid::

      graph LR
          A --> B
          B --> C

Common File Locations
---------------------

**Documentation source:**

.. code-block:: text

   docs/sphinx/
   ├── index.rst              # Main entry point
   ├── getting_started/       # Getting started guides
   ├── user_guide/           # User documentation
   ├── tutorials/            # Tutorials
   ├── api/                  # API reference
   └── development/          # Development guides

**Build output:**

.. code-block:: text

   docs/sphinx/_build/html/
   ├── index.html            # Language selector
   ├── en/                   # English docs
   └── zh_CN/                # Chinese docs

**Translation files:**

.. code-block:: text

   docs/sphinx/locale/
   └── zh_CN/
       └── LC_MESSAGES/
           ├── index.po
           ├── getting_started/
           ├── user_guide/
           └── ...

**Configuration:**

.. code-block:: text

   docs/sphinx/
   ├── conf.py               # Sphinx configuration
   ├── build_docs.py         # Build script
   ├── translate_helper.py   # Translation tool
   └── Makefile              # Make targets

Documentation Conventions
-------------------------

**File naming:**

* Use lowercase with underscores: ``my_module.rst``
* Be descriptive: ``gpio_control.rst`` not ``gpio.rst``

**Cross-references:**

* Use ``:doc:`` for documents: ``:doc:`user_guide/hal```
* Use ``:ref:`` for sections: ``:ref:`gpio-config```

**Code examples:**

* Always specify language: ``.. code-block:: c``
* Include comments for clarity
* Keep examples concise and focused

**Admonitions:**

* ``.. note::`` - Additional information
* ``.. warning::`` - Important warnings
* ``.. tip::`` - Helpful suggestions
* ``.. important::`` - Critical information

Quick Troubleshooting
----------------------

**Build fails:**

.. code-block:: bash

   # Check dependencies
   pip install -r requirements.txt

   # Clean and rebuild
   python build_docs.py --clean

**Translation not showing:**

.. code-block:: bash

   # Update .po files
   python build_docs.py --update-po

   # Rebuild specific language
   python build_docs.py --lang zh_CN

**Broken links:**

.. code-block:: bash

   # Check all links
   make linkcheck

   # View report
   cat _build/linkcheck/output.txt

**Doxygen errors:**

.. code-block:: bash

   # Run Doxygen manually
   cd ../..
   doxygen Doxyfile

   # Check for errors in output

Useful Links
------------

**Documentation:**

* :doc:`DOCUMENTATION_GUIDE` - Complete navigation guide
* :doc:`getting_started/index` - Getting started
* :doc:`development/documentation_contributing` - Contributing guide

**External:**

* `Sphinx Documentation <https://www.sphinx-doc.org/>`_
* `reStructuredText Primer <https://www.sphinx-doc.org/en/master/usage/restructuredtext/basics.html>`_
* `Sphinx i18n <https://www.sphinx-doc.org/en/master/usage/advanced/intl.html>`_
* `Doxygen Manual <https://www.doxygen.nl/manual/>`_

**Project:**

* `GitHub Repository <https://github.com/X-Gen-Lab/nexus>`_
* `Issue Tracker <https://github.com/X-Gen-Lab/nexus/issues>`_
* `Discussions <https://github.com/X-Gen-Lab/nexus/discussions>`_

Common Tasks Checklist
----------------------

**Adding new documentation:**

☐ Create .rst file in appropriate directory
☐ Add to toctree in index file
☐ Write content following conventions
☐ Build and verify locally
☐ Update translations if needed
☐ Submit pull request

**Updating existing documentation:**

☐ Edit .rst file
☐ Update cross-references if needed
☐ Build and verify changes
☐ Update translations
☐ Check for broken links
☐ Submit pull request

**Adding translation:**

☐ Run ``python build_docs.py --update-po``
☐ Edit .po files in locale/LANG/LC_MESSAGES/
☐ Run ``python translate_helper.py LANG --validate``
☐ Build translated docs
☐ Verify in browser
☐ Submit pull request

**Release documentation:**

☐ Update version in conf.py
☐ Update changelog
☐ Build all languages
☐ Run linkcheck
☐ Validate all translations
☐ Tag release
☐ Deploy to hosting

Keyboard Shortcuts (VS Code)
-----------------------------

**Editing:**

* ``Ctrl+Space`` - Autocomplete
* ``Ctrl+/`` - Toggle comment
* ``Ctrl+Shift+P`` - Command palette

**Navigation:**

* ``Ctrl+P`` - Quick file open
* ``Ctrl+Shift+F`` - Search in files
* ``F12`` - Go to definition

**Preview:**

* Install "reStructuredText" extension
* ``Ctrl+Shift+R`` - Preview RST file

Tips and Best Practices
------------------------

**Writing:**

* Keep paragraphs short and focused
* Use active voice
* Include code examples
* Add cross-references liberally

**Translation:**

* Translate content, not code
* Preserve RST markup
* Use consistent terminology
* Validate before committing

**Building:**

* Build frequently during development
* Use ``--serve`` for live preview
* Check warnings in build output
* Validate links regularly

**Maintenance:**

* Update translations when content changes
* Keep dependencies up to date
* Review and update examples
* Monitor build performance

Need Help?
----------

* Search documentation: Use search box in sidebar
* Check FAQ: :doc:`getting_started/faq`
* Ask community: `GitHub Discussions <https://github.com/X-Gen-Lab/nexus/discussions>`_
* Report issues: `GitHub Issues <https://github.com/X-Gen-Lab/nexus/issues>`_

---

**Last updated:** 2026-01-25
**Version:** 2.0
