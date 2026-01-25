Contributing
============

Thank you for your interest in contributing to Nexus! This guide covers the
contribution workflow and guidelines.

Development Environment
-----------------------

Prerequisites
~~~~~~~~~~~~~

**Windows**::

    winget install Kitware.CMake
    winget install Git.Git
    # Install Visual Studio 2019+ or Build Tools

**Linux (Ubuntu/Debian)**::

    sudo apt-get install CMake gcc g++ git

**macOS**::

    brew install CMake git

Clone and Build
~~~~~~~~~~~~~~~



    git clone https://github.com/nexus-platform/nexus.git
    cd nexus

    # Native build (for testing)
    CMake -B build -DCMAKE_BUILD_TYPE=Debug -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON
    CMake --build build --config Debug

    # Run tests
    ctest --test-dir build -C Debug --output-on-failure

Contribution Workflow
---------------------

Reporting Bugs
~~~~~~~~~~~~~~

1. Check existing issues to avoid duplicates
2. Use the bug report template
3. Include:

   - Platform and version (Windows/Linux/macOS, compiler version)
   - Steps to reproduce
   - Expected vs actual behavior
   - Relevant logs or screenshots

Suggesting Features
~~~~~~~~~~~~~~~~~~~

1. Check existing feature requests
2. Use the feature request template
3. Describe the use case and benefits

Pull Request Process
~~~~~~~~~~~~~~~~~~~~

1. Fork the repository
2. Create a feature branch: ``git checkout -b feature/my-feature``
3. Make your changes following :doc:`coding_standards`
4. Ensure tests pass: ``ctest --test-dir build -C Debug``
5. Commit with conventional commits format
6. Push and create a Pull Request

Code Style
----------

See :doc:`coding_standards` for detailed code style requirements.

Key points:

- Follow ``.clang-format`` configuration
- Use Doxygen backslash style comments (``\brief``, ``\param``)
- 80 character line limit
- 4 space indentation (no tabs)
- Pointer alignment: left (``int* ptr``)

Commit Messages
---------------

Use `Conventional Commits <https://www.conventionalcommits.org/>`_ format::

    <type>(<scope>): <subject>

    [optional body]

    [optional footer]

Types
~~~~~

- ``feat``: New feature
- ``fix``: Bug fix
- ``docs``: Documentation changes
- ``style``: Code style changes (formatting, no logic change)
- ``refactor``: Code refactoring
- ``perf``: Performance improvements
- ``test``: Adding or updating tests
- ``build``: Build system changes
- ``ci``: CI configuration changes
- ``chore``: Other changes

Examples
~~~~~~~~



    feat(hal): add PWM support for STM32F4

    fix(osal): fix mutex deadlock in FreeRTOS adapter

    docs(api): update GPIO documentation

    test(log): add unit tests for log filtering

Pre-Submit Checklist
--------------------

Before submitting a PR, verify locally::

    # 1. Build passes
    CMake -B build -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON
    CMake --build build --config Release

    # 2. Tests pass
    ctest --test-dir build -C Release --output-on-failure

    # 3. Code format check
    clang-format --dry-run --Werror hal/**/*.c hal/**/*.h

    # 4. Documentation builds without warnings
    doxygen Doxyfile

Review Process
--------------

1. Automated CI checks must pass
2. At least one maintainer approval required
3. Address all review comments
4. Squash commits if requested

CI/CD
-----

All PRs trigger GitHub Actions workflows:

- ``build.yml``: Multi-platform build (Windows, Linux, macOS) + ARM cross-compilation
- ``test.yml``: Unit tests, coverage, sanitizers, MISRA checks

Questions?
----------

Open a discussion or reach out to maintainers.
