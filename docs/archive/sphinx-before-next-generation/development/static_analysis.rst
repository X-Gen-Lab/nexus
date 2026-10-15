Static Analysis
===============

.. note::

   This page describes static analysis tools and practices for Nexus development.

Overview
--------

Static analysis helps catch bugs, security issues, and code quality problems before runtime. Nexus uses multiple static analysis tools to ensure code quality.

Tools
-----

clang-tidy
~~~~~~~~~~

**Purpose**: C/C++ linter and static analyzer

**Configuration**: ``.clang-tidy`` in project root

**Usage**:

.. code-block:: bash

   # Analyze single file
   clang-tidy hal/src/nx_hal_gpio.c

   # Analyze all files
   python scripts/tools/lint.py

**Checks Enabled**:

* Modernization checks
* Performance checks
* Readability checks
* Bug-prone pattern detection

cppcheck
~~~~~~~~

**Purpose**: Static analysis for C/C++

**Usage**:

.. code-block:: bash

   # Analyze project
   cppcheck --enable=all --inconclusive --std=c11 hal/

**Checks**:

* Memory leaks
* Null pointer dereferences
* Buffer overflows
* Uninitialized variables

MISRA C Compliance
~~~~~~~~~~~~~~~~~~

**Purpose**: Safety-critical coding standards

**Tool**: PC-lint Plus or similar

**Configuration**: Custom rule set based on MISRA C:2012

**Coverage**:

* Mandatory rules: 100% compliance target
* Required rules: 95% compliance target
* Advisory rules: Best effort

See :doc:`coding_standards` for MISRA C guidelines.

Compiler Warnings
~~~~~~~~~~~~~~~~~

**GCC/Clang Flags**:

.. code-block:: text

   -Wall
   -Wextra
   -Werror
   -Wpedantic
   -Wshadow
   -Wconversion
   -Wformat=2

**Policy**: All warnings must be fixed before merge.

Running Static Analysis
-----------------------

Automated (CI/CD)
~~~~~~~~~~~~~~~~~

Static analysis runs automatically on every pull request:

.. code-block:: yaml

   # .github/workflows/static-analysis.yml
   - name: Run clang-tidy
     run: python scripts/tools/lint.py

   - name: Run cppcheck
     run: cppcheck --enable=all --error-exitcode=1 .

Manual
~~~~~~

Run locally before committing:

.. code-block:: bash

   # Run all static analysis
   python scripts/tools/analyze.py

   # Run specific tool
   python scripts/tools/lint.py --tool clang-tidy

Common Issues
-------------

False Positives
~~~~~~~~~~~~~~~

**Problem**: Tool reports issues that aren't real problems

**Solution**:

1. Verify it's actually a false positive
2. Add suppression comment if necessary:

.. code-block:: c

   /* cppcheck-suppress nullPointer */
   *ptr = value;

3. Update tool configuration if pattern is common

Suppressing Warnings
~~~~~~~~~~~~~~~~~~~~

**Use sparingly** - only for false positives

**clang-tidy**:

.. code-block:: c

   // NOLINT(check-name)
   int value = (int)ptr;  // NOLINT(performance-no-int-to-ptr)

**cppcheck**:

.. code-block:: c

   /* cppcheck-suppress checkName */
   code_here();

Best Practices
--------------

1. **Fix warnings immediately**
   Don't let them accumulate

2. **Understand the warning**
   Don't blindly suppress

3. **Run locally**
   Catch issues before CI

4. **Keep tools updated**
   New versions catch more issues

5. **Review suppressions**
   Periodically review suppressed warnings

Integration with IDE
--------------------

VS Code
~~~~~~~

Install extensions:

* C/C++ (Microsoft)
* clangd
* Clang-Tidy

Configure in ``.vscode/settings.json``:

.. code-block:: json

   {
     "C_Cpp.codeAnalysis.clangTidy.enabled": true,
     "C_Cpp.codeAnalysis.clangTidy.path": "clang-tidy"
   }

CLion
~~~~~

Built-in clang-tidy support:

1. Settings → Editor → Inspections
2. Enable "Clang-Tidy"
3. Configure checks

Metrics
-------

Track static analysis metrics:

* Number of warnings
* Warning density (warnings per KLOC)
* Time to fix warnings
* False positive rate

Target: Zero warnings in production code.

See Also
--------

* :doc:`coding_standards` - Coding standards and style guide
* :doc:`testing` - Testing practices
* :doc:`code_review_guidelines` - Code review checklist
* :doc:`ci_cd_integration` - CI/CD pipeline

---

**Last Updated**: 2026-01-25
