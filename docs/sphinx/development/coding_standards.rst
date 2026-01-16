Coding Standards
================

Nexus follows strict coding standards for quality and safety.

C Standard
----------

- C11 for source code
- C++17 for tests

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

- Functions: ``module_action_object()`` (e.g., ``hal_gpio_init()``)
- Types: ``module_type_t`` (e.g., ``hal_status_t``)
- Macros: ``MODULE_MACRO_NAME`` (e.g., ``HAL_OK``)
- Constants: ``MODULE_CONSTANT`` (e.g., ``HAL_GPIO_PORT_MAX``)
- Static variables: ``s_variable_name`` prefix
- Global variables: ``g_variable_name`` prefix (avoid when possible)

Documentation
-------------

All public APIs must have Doxygen comments. This project uses ``\`` style
Doxygen tags (not ``@`` style).

Tag Alignment
~~~~~~~~~~~~~

All Doxygen tags must be aligned to column 20 (17 spaces after ``*``).
This ensures consistent formatting across the codebase::

    /**
     * \brief           Brief description starts at column 20
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
     * \brief           Create a mutex
     * \param[out]      handle: Pointer to store mutex handle
     * \return          OSAL_OK on success, error code otherwise
     */
    osal_status_t osal_mutex_create(osal_mutex_handle_t* handle);

Source Files (.c)
~~~~~~~~~~~~~~~~~

Source files should NOT duplicate ``\param`` and ``\return`` documentation
from headers. Use only ``\brief``, ``\details``, and ``\note``::

    /**
     * \brief           Create a mutex
     * \details         Allocates and initializes a new mutex object.
     *                  The mutex is created in unlocked state.
     * \note            Thread-safe function.
     */
    osal_status_t osal_mutex_create(osal_mutex_handle_t* handle) {
        /* Implementation */
    }

Section Comments
~~~~~~~~~~~~~~~~

Use section comments to organize code into logical blocks. The separator line
must be exactly 77 characters (``/*`` + 75 characters + ``*/``)::

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

- Using ``@`` style Doxygen tags (use ``\`` instead)
- Using ``//`` single-line comments (use ``/* */`` instead)
- Using ``/*===...===*/`` section separators (use ``/*---...---*/`` instead)
- Duplicating ``\param`` and ``\return`` in source files (only in headers)

MISRA C
-------

Code should comply with MISRA C:2012 guidelines.
