Contributing to Documentation
=============================

Thank you for your interest in improving the Nexus documentation! This guide will help you contribute effectively.

Getting Started
---------------

Prerequisites
~~~~~~~~~~~~~

Before contributing to documentation, ensure you have:

- Python 3.8 or later
- Sphinx and required extensions
- Git for version control
- A text editor (VS Code, Vim, etc.)

Setting Up the Environment
~~~~~~~~~~~~~~~~~~~~~~~~~~

1. Clone the repository:

.. code-block:: bash

    git clone https://github.com/nexus-team/nexus.git
    cd nexus

2. Install documentation dependencies:

.. code-block:: bash

    pip install -r docs/sphinx/requirements.txt

3. Build the documentation locally:

.. code-block:: bash

    cd docs/sphinx
    python build_docs.py

4. View the documentation:

Open ``_build/html/index.html`` in your web browser.

Documentation Structure
-----------------------

The documentation is organized as follows:

.. code-block:: text

    docs/sphinx/
    ├── getting_started/     # Installation and quickstart guides
    ├── user_guide/          # User guides for modules and features
    ├── platform_guides/     # Platform-specific documentation
    ├── api/                 # API reference (auto-generated)
    ├── tutorials/           # Step-by-step tutorials
    ├── development/         # Development guides
    ├── reference/           # Reference materials
    └── _templates/          # Documentation templates

Types of Contributions
----------------------

We welcome the following types of documentation contributions:

Fixing Errors
~~~~~~~~~~~~~

- Typos and grammar mistakes
- Broken links
- Incorrect code examples
- Outdated information

Improving Existing Documentation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- Adding missing details
- Clarifying confusing sections
- Adding diagrams and illustrations
- Improving code examples

Adding New Documentation
~~~~~~~~~~~~~~~~~~~~~~~~

- New tutorials
- Platform guides
- Module documentation
- API documentation

Translation
~~~~~~~~~~~

- Translating documentation to Chinese
- Maintaining translation consistency
- Updating translations when English docs change

Writing Guidelines
------------------

Style Guide
~~~~~~~~~~~

**Tone and Voice**

- Use clear, concise language
- Write in present tense
- Use active voice
- Be direct and specific

**Formatting**

- Use consistent heading levels
- Keep line length under 120 characters
- Use code blocks with language specification
- Include alt text for images

**Terminology**

Use consistent terminology throughout:

- CMake (not CMake)
- Kconfig (not Kconfig)
- FreeRTOS (not FreeRTOS)
- STM32 (not STM32)
- GPIO, UART, SPI, I2C (all caps)

Code Examples
~~~~~~~~~~~~~

All code examples should:

- Be complete and runnable when possible
- Include necessary headers and includes
- Use meaningful variable names
- Include comments for complex sections
- Follow the project's coding standards

Example:

.. code-block:: c

    #include "hal/nx_gpio.h"

    /* Initialize GPIO for LED */
    nx_gpio_config_t led_config = {
        .mode = NX_GPIO_MODE_OUTPUT_PP,
        .pull = NX_GPIO_PULL_NONE,
        .speed = NX_GPIO_SPEED_LOW,
    };

    nx_gpio_t* led = nx_factory_gpio(NX_GPIO_PORT_A, 5);
    led->init(led, &led_config);

    /* Turn on LED */
    led->write(led, 1);

RST Syntax
~~~~~~~~~~

**Headings**

Use consistent underline characters:

.. code-block:: rst

    Level 1 Heading
    ===============

    Level 2 Heading
    ---------------

    Level 3 Heading
    ~~~~~~~~~~~~~~~

    Level 4 Heading
    ^^^^^^^^^^^^^^^

**Code Blocks**

Always specify the language:

.. code-block:: rst

    .. code-block:: c

        int main(void) {
            return 0;
        }

**Lists**

.. code-block:: rst

    - Bullet item 1
    - Bullet item 2

    1. Numbered item 1
    2. Numbered item 2

**Links**

.. code-block:: rst

    :doc:`../user_guide/hal`  # Internal link
    `External Link <https://example.com>`_  # External link

**Tables**

.. code-block:: rst

    .. list-table::
       :header-rows: 1
       :widths: 30 70

       * - Column 1
         - Column 2
       * - Value 1
         - Value 2

**Diagrams**

.. code-block:: rst

    .. mermaid::
       :alt: Workflow diagram showing process from start to end

       graph TD
           A[Start] --> B[Process]
           B --> C[End]

Using Templates
---------------

We provide templates for common documentation types:

Module Documentation
~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

    cp docs/sphinx/_templates/module_template.rst docs/sphinx/user_guide/my_module.rst

Tutorial Documentation
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

    cp docs/sphinx/_templates/tutorial_template.rst docs/sphinx/tutorials/my_tutorial.rst

Platform Guide
~~~~~~~~~~~~~~

.. code-block:: bash

    cp docs/sphinx/_templates/platform_guide_template.rst docs/sphinx/platform_guides/my_platform.rst

See the template files in ``docs/sphinx/_templates/`` for detailed template usage.

Contribution Workflow
---------------------

1. Create a Branch
~~~~~~~~~~~~~~~~~~

.. code-block:: bash

    git checkout -b docs/improve-hal-guide

Use descriptive branch names with ``docs/`` prefix.

2. Make Changes
~~~~~~~~~~~~~~~

- Edit the relevant RST files
- Add new files if needed
- Update the table of contents (``index.rst``)

3. Build and Test
~~~~~~~~~~~~~~~~~

.. code-block:: bash

    cd docs/sphinx
    python build_docs.py

Check for:

- Build warnings or errors
- Broken links
- Formatting issues
- Code example correctness

4. Run Quality Checks
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

    # Check style
    python check_style.py

    # Check spelling
    python check_spelling.py

    # Validate code examples
    python validate_code_examples.py

5. Commit Changes
~~~~~~~~~~~~~~~~~

.. code-block:: bash

    git add docs/sphinx/user_guide/my_module.rst
    git commit -m "docs: improve HAL GPIO documentation

    - Add missing configuration examples
    - Clarify interrupt handling
    - Fix code example errors"

Use conventional commit format:

- ``docs: description`` for documentation changes
- ``docs(module): description`` for module-specific changes

6. Push and Create Pull Request
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

    git push origin docs/improve-hal-guide

Then create a pull request on GitHub with:

- Clear title describing the change
- Description of what was changed and why
- Reference to any related issues

Review Process
--------------

What to Expect
~~~~~~~~~~~~~~

1. **Automated Checks**: CI will run style checks, spell checking, and link validation
2. **Peer Review**: Maintainers will review your changes
3. **Feedback**: You may receive suggestions for improvements
4. **Approval**: Once approved, your changes will be merged

Responding to Feedback
~~~~~~~~~~~~~~~~~~~~~~

- Address all review comments
- Ask questions if something is unclear
- Make requested changes promptly
- Update your pull request with fixes

Translation Guidelines
----------------------

If you're contributing translations:

Workflow
~~~~~~~~

1. Extract translatable strings:

.. code-block:: bash

    sphinx-build -b gettext . _build/gettext

2. Update translation files:

.. code-block:: bash

    sphinx-intl update -p _build/gettext -l zh_CN

3. Edit ``.po`` files in ``locale/zh_CN/LC_MESSAGES/``

4. Build translated documentation:

.. code-block:: bash

    sphinx-build -b html -D language=zh_CN . _build/html/zh_CN

Translation Best Practices
~~~~~~~~~~~~~~~~~~~~~~~~~~

- Maintain consistent terminology
- Preserve formatting and markup
- Keep technical terms in English when appropriate
- Test the translated documentation

Common Issues
-------------

Build Errors
~~~~~~~~~~~~

**Issue**: ``WARNING: document isn't included in any toctree``

**Solution**: Add the document to an ``index.rst`` file:

.. code-block:: rst

    .. toctree::
       :maxdepth: 2

       my_new_document

**Issue**: ``ERROR: Unknown directive type "code-block"``

**Solution**: Ensure proper indentation and blank lines:

.. code-block:: rst

    .. code-block:: c

       int main(void) {
           return 0;
       }

Link Errors
~~~~~~~~~~~

**Issue**: ``WARNING: undefined label``

**Solution**: Check that the referenced document exists and is in the toctree.

Style Violations
~~~~~~~~~~~~~~~~

**Issue**: Heading underline length mismatch

**Solution**: Ensure underline matches heading length exactly:

.. code-block:: rst

    My Heading
    ==========

Getting Help
------------

If you need help:

- Check existing documentation in ``docs/sphinx/``
- Review this contributing guide
- Ask questions in pull request comments
- Contact maintainers via GitHub issues

Resources
---------

- `Sphinx Documentation <https://www.sphinx-doc.org/>`_
- `reStructuredText Primer <https://www.sphinx-doc.org/en/master/usage/restructuredtext/basics.html>`_
- `Breathe Documentation <https://breathe.readthedocs.io/>`_
- `Mermaid Documentation <https://mermaid-js.github.io/>`_

Thank You!
----------

Your contributions help make Nexus better for everyone. We appreciate your time and effort!
