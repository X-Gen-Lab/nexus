Coding Standards
================

Nexus follows strict coding standards for quality and safety.

C Standard
----------

- C11 for source code
- C++17 for tests

Naming Conventions
------------------

- Functions: ``module_action_object()`` (e.g., ``hal_gpio_init()``)
- Types: ``module_type_t`` (e.g., ``hal_status_t``)
- Macros: ``MODULE_MACRO_NAME`` (e.g., ``HAL_OK``)
- Constants: ``MODULE_CONSTANT`` (e.g., ``HAL_GPIO_PORT_MAX``)

Documentation
-------------

All public APIs must have Doxygen comments::

    /**
     * \\brief           Brief description
     * \\param[in]       param: Parameter description
     * \\return          Return value description
     */

MISRA C
-------

Code should comply with MISRA C:2012 guidelines.
