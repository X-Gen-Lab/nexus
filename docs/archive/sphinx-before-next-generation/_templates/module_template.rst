Module Name
===========

.. Brief description of the module (1-2 sentences)

Overview
--------

.. Detailed description of what the module does and why it exists

Key Features
~~~~~~~~~~~~

- Feature 1: Description
- Feature 2: Description
- Feature 3: Description

Architecture
------------

.. Describe the module's architecture and design

.. mermaid::
   :alt: Module architecture diagram showing the relationship between Component A, B, and C

   graph TD
       A[Component A] --> B[Component B]
       B --> C[Component C]

Components
~~~~~~~~~~

Component A
^^^^^^^^^^^

.. Description of Component A

Component B
^^^^^^^^^^^

.. Description of Component B

API Reference
-------------

Core Functions
~~~~~~~~~~~~~~

.. doxygenfunction:: module_init
   :project: nexus

.. doxygenfunction:: module_deinit
   :project: nexus

Configuration
~~~~~~~~~~~~~

.. doxygenfunction:: module_configure
   :project: nexus

Data Types
~~~~~~~~~~

.. doxygenstruct:: module_config_t
   :project: nexus
   :members:

.. doxygenstruct:: module_handle_t
   :project: nexus
   :members:

Usage Examples
--------------

Basic Usage
~~~~~~~~~~~

.. code-block:: c

    #include "module/module.h"

    int main(void) {
        /* Initialize module */
        module_config_t config = {
            .param1 = VALUE1,
            .param2 = VALUE2,
        };

        module_handle_t handle;
        module_status_t status = module_init(&config, &handle);

        if (status != MODULE_OK) {
            /* Handle error */
            return -1;
        }

        /* Use module */
        module_operation(handle);

        /* Clean up */
        module_deinit(handle);

        return 0;
    }

Advanced Usage
~~~~~~~~~~~~~~

.. code-block:: c

    /* Advanced example showing more complex usage */

    /* Example code here */

Configuration Options
---------------------

The module can be configured using Kconfig options:

.. list-table::
   :header-rows: 1
   :widths: 30 20 50

   * - Option
     - Default
     - Description
   * - ``CONFIG_MODULE_ENABLE``
     - ``y``
     - Enable the module
   * - ``CONFIG_MODULE_BUFFER_SIZE``
     - ``1024``
     - Size of internal buffer
   * - ``CONFIG_MODULE_TIMEOUT``
     - ``1000``
     - Default timeout in milliseconds

Error Handling
--------------

The module uses the following error codes:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Error Code
     - Description
   * - ``MODULE_OK``
     - Operation successful
   * - ``MODULE_ERROR_INVALID_PARAM``
     - Invalid parameter provided
   * - ``MODULE_ERROR_TIMEOUT``
     - Operation timed out
   * - ``MODULE_ERROR_NO_MEMORY``
     - Insufficient memory

Thread Safety
-------------

.. note::

   All module functions are thread-safe unless otherwise noted.

Performance Considerations
--------------------------

.. Describe performance characteristics and optimization tips

- Consideration 1
- Consideration 2
- Consideration 3

Troubleshooting
---------------

Common Issues
~~~~~~~~~~~~~

**Issue 1: Description**

Solution: Explanation of how to resolve

**Issue 2: Description**

Solution: Explanation of how to resolve

See Also
--------

- :doc:`related_module_1`
- :doc:`related_module_2`
- :doc:`../tutorials/module_tutorial`
