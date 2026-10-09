Coding Standards
================

Nexus follows strict coding standards for quality and safety.

C Standard
----------

- C11 for source code
- C11 for maintained contract tests; C++17 for external ABI consumer checks

Code Formatting
---------------

The project uses clang-format for automatic code formatting. Run ``clang-format``
before committing code.

See also: ``.kiro/steering/comment-standards.md`` for detailed comment guidelines.

Indentation
~~~~~~~~~~~

- Use 4 spaces for indentation (no tabs)
- Indent case labels inside switch statements
- Do not indent preprocessor directives

Line Length
~~~~~~~~~~~

- Maximum line length: 80 characters
- Break long lines at appropriate points

Braces
~~~~~~

Use attached braces (K&R style)::

    if (condition) {
        /* code */
    } else {
        /* code */
    }

    void function(void) {
        /* code */
    }

Spaces
~~~~~~

- Space after control keywords: ``if (``, ``for (``, ``while (``
- No space after function names: ``func()``
- No space after casts: ``(int)value``
- Two spaces before trailing comments
- Space around binary operators: ``a + b``, ``x = y``

Pointer Alignment
~~~~~~~~~~~~~~~~~

Pointers align to the left (with the type)::

    int* ptr;           /* Correct */
    char* str;          /* Correct */

Empty Lines
~~~~~~~~~~~

- Maximum one consecutive empty line
- Use empty lines to separate logical sections

Naming Conventions
------------------

- Functions: ``module_action_object()`` (e.g., ``nx_gpio_port_write()``)
- Types: ``module_type_t`` (e.g., ``nx_result_t``)
- Macros: ``MODULE_MACRO_NAME`` (e.g., ``NX_SUCCESS``)
- Constants: ``MODULE_CONSTANT`` (e.g., ``NEXUS_UART0_SELECTED``)
- Static variables: ``s_variable_name`` prefix
- Global variables: ``g_variable_name`` prefix (avoid when possible)

Documentation
-------------

All public APIs must have Doxygen comments. This project uses ``\\`` style
Doxygen tags (not ``@`` style).

Tag Alignment
~~~~~~~~~~~~~

Align the description after each Doxygen tag as shown below. Excluding any
surrounding code indentation, the description starts at column 21: the tag and
its padding occupy 17 characters after the opening comment star::

    /**
     * \brief           Brief description starts at column 21
     * \param[in]       param: Parameter description
     * \param[out]      result: Output parameter description
     * \return          Return value description
     */

File Header Format
~~~~~~~~~~~~~~~~~~

Every source file must have a file header comment.

**Header files (.h)** - Minimal format::

    /**
     * \file            filename.h
     * \brief           Brief description of the file
     * \author          Nexus Team
     */

**Source files (.c)** - Full format with version and copyright::

    /**
     * \file            filename.c
     * \brief           Brief description of the file
     * \author          Nexus Team
     * \version         1.0.0
     * \date            2026-01-12
     *
     * \copyright       Copyright (c) 2026 Nexus Team
     *
     * \details         Detailed description of the file contents
     *                  and purpose. Can span multiple lines.
     */

Header Files (.h)
~~~~~~~~~~~~~~~~~

Header files contain full API documentation including parameters and return values::

    /**
     * \brief           Prepare an exclusively owned reusable request.
     * \param[in,out]   request: Quiescent caller storage.
     * \param[in]       deadline: Absolute monotonic deadline.
     * \return          Success or STATE while an earlier borrow remains.
     */
    nx_result_t nx_request_prepare(nx_request_t* request, nx_time_us_t deadline);

Source Files (.c)
~~~~~~~~~~~~~~~~~

Source definitions explain hardware ordering and invariants without duplicating
``\param`` or ``\return`` from the header. Use ``\brief``, ``\details`` and
``\note`` as needed::

    /** \brief           Restore the saved incoming interrupt mask. */
    void nx_arch_irq_restore(nx_arch_irq_state_t state) {
        /* Implementation */
    }

Section Comments
~~~~~~~~~~~~~~~~

Use section comments to organize code into logical blocks. The separator line
must be exactly 79 characters (``/*`` + 75 characters + ``*/``), preserving the
existing template::

    /*---------------------------------------------------------------------------*/
    /* Section Name                                                              */
    /*---------------------------------------------------------------------------*/

**Important**: Do NOT use ``/*===...===*/`` style separators. Always use
``/*---...---*/`` for consistency.

Inline Comments
~~~~~~~~~~~~~~~

Use ``/* comment */`` style for inline comments, not ``//``::

    int value = 0;  /* Initialize to zero */

Macro Comments
~~~~~~~~~~~~~~

Use Doxygen block comments or inline comments for macros::

    /**
     * \brief           Maximum buffer size
     */
    #define MAX_BUFFER_SIZE 256

    /* Or use inline style */
    #define MAX_BUFFER_SIZE 256  /**< Maximum buffer size */

Static Functions
~~~~~~~~~~~~~~~~

Static functions use simplified comments with only ``\brief``::

    /**
     * \brief           Internal helper function description
     */
    static void internal_helper(void) {
        /* Implementation */
    }

Prohibited Practices
~~~~~~~~~~~~~~~~~~~~

The following practices are NOT allowed:

- Using ``@`` style Doxygen tags (use ``\\`` instead)
- Using ``//`` single-line comments (use ``/* */`` instead)
- Using ``/*===...===*/`` section separators (use ``/*---...---*/`` instead)
- Duplicating ``\param`` and ``\return`` in source files (only in headers)

Ownership and evidence
----------------------

Public declarations specify context, deadlines, retained storage and failure
state. Describe why a volatile read, ordering barrier or drain is necessary.
``SETTLED`` is the provider's final request access. Cancellation never permits
reclaiming a borrowed buffer before settlement or proved controlled recovery.

Changed files pass whole-file formatting and mechanical comment checks. Install
local Git hooks with ``python scripts/setup/install_dev_tools.py``. The commit
and CI gates share ``scripts/ci/style_gate.py``; no hook bypass is a validation
result. Actual warning/analyzer and behavioral evidence is separate from style.

Selected analyzer checks do not establish MISRA certification or industrial
functional-safety qualification.
