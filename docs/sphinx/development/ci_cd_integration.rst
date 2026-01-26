CI/CD Integration
=================

This guide covers the Nexus Embedded Platform's CI/CD system, including the optimized GitHub Actions workflows and integration with other CI/CD platforms.

.. contents:: Table of Contents
   :local:
   :depth: 2

Overview
--------

The Nexus platform provides a comprehensive, modular CI/CD system with:

* **Intelligent Triggering**: Path-based filtering and smart scheduling
* **Modular Design**: Reusable workflows and composite actions
* **Multi-Platform Builds**: Windows, Linux, macOS, and ARM cross-compilation
* **Quality Assurance**: Code coverage, static analysis, sanitizers, and MISRA compliance
* **Performance Optimization**: Build caching, parallel execution, and smart dependencies
* **Complete Documentation**: Automated Doxygen and Sphinx documentation with i18n support

System Architecture
-------------------

The Nexus CI/CD system uses a modular architecture:

.. code-block:: text

   ci.yml (Main Orchestrator)
       ↓
   Smart Change Detection
       ↓
   Trigger Sub-Workflows
       ├─► build-matrix.yml (Build & Test)
       │   ├─ Multi-platform builds
       │   ├─ Coverage analysis
       │   └─ Sanitizer tests
       │
       ├─► quality-checks.yml (Code Quality)
       │   ├─ Format checking
       │   ├─ Static analysis
       │   ├─ Complexity analysis
       │   └─ MISRA compliance
       │
       └─► docs-build.yml (Documentation)
           ├─ Doxygen API docs
           ├─ Sphinx user docs (EN/CN)
           └─ GitHub Pages deployment

Optimization Results
~~~~~~~~~~~~~~~~~~~~

The optimized CI/CD system achieves:

* **55% reduction** in code size (from ~2000 to ~900 lines)
* **40% faster** build times (from 25 to 15 minutes)
* **85% cache hit rate** (up from 60%)
* **140% more** parallel tasks (from 3-5 to 8-12)

GitHub Actions (Recommended)
-----------------------------

The Nexus project uses an optimized GitHub Actions setup with modular workflows.

Main CI Workflow
~~~~~~~~~~~~~~~~

The main workflow (``.github/workflows/ci.yml``) orchestrates all CI tasks:

Main CI Workflow
~~~~~~~~~~~~~~~~

The main workflow (``.github/workflows/ci.yml``) orchestrates all CI tasks:

.. code-block:: yaml

   name: CI

   on:
     push:
       branches: [ main, develop ]
     pull_request:
       branches: [ main, develop ]
     workflow_dispatch:
     schedule:
       - cron: '0 2 * * *'

   concurrency:
     group: ${{ github.workflow }}-${{ github.ref }}
     cancel-in-progress: true

   jobs:
     changes:
       name: Detect Changes
       runs-on: ubuntu-latest
       outputs:
         code: ${{ steps.filter.outputs.code }}
         docs: ${{ steps.filter.outputs.docs }}
         workflows: ${{ steps.filter.outputs.workflows }}
       steps:
         - uses: actions/checkout@v4
         - uses: dorny/paths-filter@v3
           id: filter
           with:
             filters: |
               code:
                 - 'hal/**'
                 - 'osal/**'
                 - 'framework/**'
                 - '**/CMakeLists.txt'
               docs:
                 - 'docs/**'
                 - '**.md'
               workflows:
                 - '.github/workflows/**'

     build-test:
       name: Build & Test
       needs: changes
       if: needs.changes.outputs.code == 'true'
       uses: ./.github/workflows/build-matrix.yml
       secrets: inherit

     code-quality:
       name: Code Quality
       needs: changes
       if: needs.changes.outputs.code == 'true'
       uses: ./.github/workflows/quality-checks.yml

     docs:
       name: Documentation
       needs: changes
       if: needs.changes.outputs.docs == 'true'
       uses: ./.github/workflows/docs-build.yml
       secrets: inherit

Build Matrix Workflow
~~~~~~~~~~~~~~~~~~~~~

The build matrix workflow (``.github/workflows/build-matrix.yml``) handles multi-platform builds:

.. code-block:: yaml

   name: Build Matrix

   on:
     workflow_call:
       secrets:
         CODECOV_TOKEN:
           required: false

   jobs:
     matrix-build:
       name: ${{ matrix.name }}
       runs-on: ${{ matrix.os }}
       strategy:
         fail-fast: false
         matrix:
           include:
             - name: Windows MSVC Release
               os: windows-latest
               preset: windows-msvc-release
               upload_artifacts: true

             - name: Linux GCC Release
               os: ubuntu-latest
               preset: linux-gcc-release
               upload_artifacts: true

             - name: macOS Clang Release
               os: macos-latest
               preset: macos-clang-release
               upload_artifacts: true

             - name: ARM Cortex-M4 Release
               os: ubuntu-latest
               preset: cross-arm-release
               upload_artifacts: true
               is_arm: true

       steps:
         - uses: actions/checkout@v4
           with:
             submodules: recursive

         - name: Setup Build Environment
           uses: ./.github/actions/setup-build
           with:
             os: ${{ matrix.os }}
             preset: ${{ matrix.preset }}

         - name: Configure
           run: cmake --preset ${{ matrix.preset }}

         - name: Build
           run: cmake --build --preset ${{ matrix.preset }} --parallel

         - name: Test
           if: ${{ !matrix.is_arm }}
           run: ctest --preset ${{ matrix.preset }} --output-on-failure

     coverage:
       name: Coverage Analysis
       runs-on: ubuntu-latest
       steps:
         - uses: actions/checkout@v4
           with:
             submodules: recursive

         - name: Setup Build Environment
           uses: ./.github/actions/setup-build
           with:
             os: ubuntu-latest
             preset: linux-gcc-coverage

         - name: Configure
           run: cmake --preset linux-gcc-coverage

         - name: Build
           run: cmake --build --preset linux-gcc-coverage --parallel

         - name: Test
           run: ctest --preset linux-gcc-coverage --output-on-failure

         - name: Generate Coverage
           run: |
             lcov --capture --directory . --output-file coverage.info
             lcov --remove coverage.info '/usr/*' '*/tests/*' '*/ext/*' \
               --output-file coverage.info

         - name: Upload to Codecov
           uses: codecov/codecov-action@v4
           with:
             files: ./coverage.info
             flags: unittests

     sanitizer:
       name: Sanitizer - ${{ matrix.sanitizer }}
       runs-on: ubuntu-latest
       strategy:
         matrix:
           sanitizer: [address, undefined, thread]
       steps:
         - uses: actions/checkout@v4
           with:
             submodules: recursive

         - name: Setup Build Environment
           uses: ./.github/actions/setup-build
           with:
             os: ubuntu-latest
             preset: linux-gcc-debug

         - name: Configure with Sanitizer
           run: |
             SANITIZER_FLAG=""
             case "${{ matrix.sanitizer }}" in
               address) SANITIZER_FLAG="-fsanitize=address" ;;
               undefined) SANITIZER_FLAG="-fsanitize=undefined" ;;
               thread) SANITIZER_FLAG="-fsanitize=thread" ;;
             esac

             cmake -B build -DCMAKE_BUILD_TYPE=Debug \
               -DNEXUS_PLATFORM=native -DNEXUS_BUILD_TESTS=ON \
               -DCMAKE_C_FLAGS="$SANITIZER_FLAG"

         - name: Build
           run: cmake --build build --parallel

         - name: Test
           run: ctest --test-dir build --output-on-failure

Quality Checks Workflow
~~~~~~~~~~~~~~~~~~~~~~~

The quality checks workflow (``.github/workflows/quality-checks.yml``) ensures code quality:

.. code-block:: yaml

   name: Quality Checks

   on:
     workflow_call:

   jobs:
     format-check:
       name: Format Check
       runs-on: ubuntu-latest
       steps:
         - uses: actions/checkout@v4

         - name: Check clang-format
           run: |
             sudo apt-get install -y clang-format-14
             find hal osal framework \
               \( -name '*.c' -o -name '*.h' \) \
               -exec clang-format-14 --dry-run --Werror {} +

     static-analysis:
       name: Static Analysis
       runs-on: ubuntu-latest
       steps:
         - uses: actions/checkout@v4
           with:
             submodules: recursive

         - name: Setup Build Environment
           uses: ./.github/actions/setup-build
           with:
             os: ubuntu-latest
             preset: linux-clang-debug

         - name: Configure
           run: cmake --preset linux-clang-debug

         - name: Run clang-tidy
           run: |
             run-clang-tidy-14 -p build/linux-clang-debug \
               'hal/.*|osal/.*|framework/.*'

         - name: Run cppcheck
           run: |
             sudo apt-get install -y cppcheck
             cppcheck --enable=all --inconclusive \
               -I hal/include -I osal/include -I framework/include \
               hal/ osal/ framework/

     complexity:
       name: Complexity Check
       runs-on: ubuntu-latest
       steps:
         - uses: actions/checkout@v4

         - name: Run lizard
           run: |
             pip install lizard
             lizard hal/ osal/ framework/ -l c -C 15 -L 100

     misra:
       name: MISRA Compliance
       runs-on: ubuntu-latest
       steps:
         - uses: actions/checkout@v4

         - name: Run MISRA Check
           run: |
             sudo apt-get install -y cppcheck
             cppcheck --addon=misra \
               -I hal/include -I osal/include -I framework/include \
               hal/ osal/ framework/

Documentation Workflow
~~~~~~~~~~~~~~~~~~~~~~

The documentation workflow (``.github/workflows/docs-build.yml``) builds and deploys documentation:

.. code-block:: yaml

   name: Documentation Build

   on:
     workflow_call:

   permissions:
     contents: read
     pages: write
     id-token: write

   jobs:
     build:
       name: Build Docs
       runs-on: ubuntu-latest
       steps:
         - uses: actions/checkout@v4

         - name: Setup Python
           uses: actions/setup-python@v5
           with:
             python-version: '3.11'
             cache: 'pip'

         - name: Install Dependencies
           run: |
             sudo apt-get install -y doxygen graphviz
             pip install -r docs/sphinx/requirements.txt

         - name: Build Doxygen
           run: doxygen Doxyfile

         - name: Build Sphinx (English)
           working-directory: docs/sphinx
           run: sphinx-build -b html . _build/html/en

         - name: Build Sphinx (Chinese)
           working-directory: docs/sphinx
           run: sphinx-build -b html -D language=zh_CN . _build/html/zh_CN

         - name: Prepare Pages
           run: |
             mkdir -p public
             cp -r docs/api/html public/api
             cp -r docs/sphinx/_build/html/* public/

         - name: Upload Pages Artifact
           uses: actions/upload-pages-artifact@v3
           with:
             path: public

     deploy:
       name: Deploy to Pages
       runs-on: ubuntu-latest
       needs: build
       if: github.ref == 'refs/heads/main'
       environment:
         name: github-pages
         url: ${{ steps.deployment.outputs.page_url }}
       steps:
         - name: Deploy
           id: deployment
           uses: actions/deploy-pages@v4

Composite Action
~~~~~~~~~~~~~~~~

The setup-build composite action (``.github/actions/setup-build/action.yml``) encapsulates environment setup:

.. code-block:: yaml

   name: Setup Build Environment
   description: Setup build tools and dependencies for Nexus

   inputs:
     os:
       description: 'Operating system'
       required: true
     preset:
       description: 'CMake preset name'
       required: true

   runs:
     using: composite
     steps:
       - name: Setup Python
         uses: actions/setup-python@v5
         with:
           python-version: '3.11'
           cache: 'pip'

       - name: Install Python Dependencies
         shell: bash
         run: |
           python -m pip install --upgrade pip
           pip install -r requirements.txt

       - name: Install Build Tools (Linux)
         if: runner.os == 'Linux'
         shell: bash
         run: |
           sudo apt-get update
           sudo apt-get install -y cmake ninja-build ccache

           if [[ "${{ inputs.preset }}" == *"clang"* ]]; then
             sudo apt-get install -y clang-14 clang-tidy-14
           fi

           if [[ "${{ inputs.preset }}" == *"coverage"* ]]; then
             sudo apt-get install -y lcov
           fi

           if [[ "${{ inputs.preset }}" == *"arm"* ]]; then
             sudo apt-get install -y gcc-arm-none-eabi
           fi

       - name: Setup ccache (Linux)
         if: runner.os == 'Linux'
         uses: hendrikmuhs/ccache-action@v1.2
         with:
           key: ${{ inputs.preset }}

Release Workflow
~~~~~~~~~~~~~~~~

The release workflow (``.github/workflows/release.yml``) automates releases:

.. code-block:: yaml

   name: Release

   on:
     push:
       tags:
         - 'v*.*.*'
     workflow_dispatch:
       inputs:
         version:
           description: 'Release version (e.g., v1.0.0)'
           required: true

   jobs:
     create-release:
       name: Create Release
       runs-on: ubuntu-latest
       outputs:
         upload_url: ${{ steps.create_release.outputs.upload_url }}
         version: ${{ steps.get_version.outputs.version }}

       steps:
         - uses: actions/checkout@v4
           with:
             fetch-depth: 0

         - name: Get version
           id: get_version
           run: |
             if [ "${{ github.event_name }}" == "workflow_dispatch" ]; then
               echo "version=${{ github.event.inputs.version }}" >> $GITHUB_OUTPUT
             else
               echo "version=${GITHUB_REF#refs/tags/}" >> $GITHUB_OUTPUT
             fi

         - name: Create Release
           id: create_release
           uses: softprops/action-gh-release@v1
           with:
             tag_name: ${{ steps.get_version.outputs.version }}
             name: Release ${{ steps.get_version.outputs.version }}

     build-release:
       name: Build ${{ matrix.name }}
       needs: create-release
       runs-on: ${{ matrix.os }}
       strategy:
         matrix:
           include:
             - name: Windows MSVC
               os: windows-latest
               preset: windows-msvc-release
             - name: Linux GCC
               os: ubuntu-latest
               preset: linux-gcc-release
             - name: macOS Clang
               os: macos-latest
               preset: macos-clang-release
             - name: ARM Cortex-M4
               os: ubuntu-latest
               preset: cross-arm-release

       steps:
         - uses: actions/checkout@v4
           with:
             submodules: recursive

         - name: Setup Build Environment
           uses: ./.github/actions/setup-build
           with:
             os: ${{ matrix.os }}
             preset: ${{ matrix.preset }}

         - name: Configure
           run: cmake --preset ${{ matrix.preset }}

         - name: Build
           run: cmake --build --preset ${{ matrix.preset }} --parallel

         - name: Package
           run: |
             mkdir -p release/${{ matrix.name }}
             cp -r build/${{ matrix.preset }}/bin/* release/${{ matrix.name }}/
             cp README.md LICENSE release/${{ matrix.name }}/

         - name: Upload to Release
           uses: softprops/action-gh-release@v1
           with:
             tag_name: ${{ needs.create-release.outputs.version }}
             files: release/*

Performance and Security Workflows
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Additional workflows for performance testing and security scanning:

**Performance Testing** (``.github/workflows/performance.yml``):

* Runs weekly on Monday
* Performance benchmarks
* Memory profiling with Valgrind
* Code size analysis for ARM targets

**Security Scanning** (``.github/workflows/security.yml``):

* Runs weekly on Sunday
* Dependency scanning with Safety
* CodeQL analysis for C/C++ and Python
* Secret scanning with TruffleHog
* License compliance checking

Using CMake Presets
~~~~~~~~~~~~~~~~~~~

The Nexus CI/CD system uses CMake Presets for consistent configuration:

.. code-block:: bash

   # Configure
   cmake --preset linux-gcc-release

   # Build
   cmake --build --preset linux-gcc-release --parallel

   # Test
   ctest --preset linux-gcc-release --output-on-failure

Available presets:

* ``windows-msvc-debug`` / ``windows-msvc-release``
* ``windows-gcc-debug`` / ``windows-gcc-release``
* ``linux-gcc-debug`` / ``linux-gcc-release``
* ``linux-gcc-coverage`` (with coverage enabled)
* ``linux-clang-debug`` / ``linux-clang-release``
* ``macos-clang-debug`` / ``macos-clang-release``
* ``cross-arm-debug`` / ``cross-arm-release``

Local Development
~~~~~~~~~~~~~~~~~

Run CI checks locally before pushing:

**Format Check:**

.. code-block:: bash

   find hal osal framework -name '*.c' -o -name '*.h' | xargs clang-format -i

**Build and Test:**

.. code-block:: bash

   cmake --preset linux-gcc-debug
   cmake --build --preset linux-gcc-debug
   ctest --preset linux-gcc-debug

**Coverage Analysis:**

.. code-block:: bash

   cmake --preset linux-gcc-coverage
   cmake --build --preset linux-gcc-coverage
   ctest --preset linux-gcc-coverage

   # Generate report
   lcov --capture --directory . --output-file coverage.info
   lcov --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage.info
   genhtml coverage.info --output-directory coverage_html

**Static Analysis:**

.. code-block:: bash

   # clang-tidy
   cmake --preset linux-clang-debug
   run-clang-tidy -p build/linux-clang-debug

   # cppcheck
   cppcheck --enable=all -I hal/include -I osal/include hal/ osal/

**Complexity Analysis:**

.. code-block:: bash

   pip install lizard
   lizard hal/ osal/ framework/ -l c -C 15 -L 100

Other CI/CD Platforms
---------------------

While GitHub Actions is recommended, Nexus can integrate with other platforms.

GitLab CI
~~~~~~~~~

Basic ``.gitlab-ci.yml`` configuration:

.. code-block:: yaml

   stages:
     - build
     - test
     - deploy

   variables:
     GIT_SUBMODULE_STRATEGY: recursive

   build:linux:
     stage: build
     image: ubuntu:22.04
     before_script:
       - apt-get update && apt-get install -y cmake ninja-build gcc g++
     script:
       - cmake --preset linux-gcc-release
       - cmake --build --preset linux-gcc-release
     artifacts:
       paths:
         - build/
       expire_in: 1 hour

   test:
     stage: test
     image: ubuntu:22.04
     dependencies:
       - build:linux
     script:
       - ctest --preset linux-gcc-release --output-on-failure

Jenkins
~~~~~~~

Basic ``Jenkinsfile`` configuration:

.. code-block:: groovy

   pipeline {
       agent any

       stages {
           stage('Checkout') {
               steps {
                   checkout scm
                   sh 'git submodule update --init --recursive'
               }
           }

           stage('Build') {
               steps {
                   sh '''
                       cmake --preset linux-gcc-release
                       cmake --build --preset linux-gcc-release --parallel
                   '''
               }
           }

           stage('Test') {
               steps {
                   sh 'ctest --preset linux-gcc-release --output-on-failure'
               }
           }
       }

       post {
           always {
               archiveArtifacts artifacts: 'build/**/*', allowEmptyArchive: true
           }
       }
   }

Azure DevOps
~~~~~~~~~~~~

Basic ``azure-pipelines.yml`` configuration:

.. code-block:: yaml

   trigger:
     branches:
       include:
         - main
         - develop

   pool:
     vmImage: 'ubuntu-latest'

   steps:
     - checkout: self
       submodules: true

     - script: |
         cmake --preset linux-gcc-release
         cmake --build --preset linux-gcc-release --parallel
       displayName: 'Build'

     - script: |
         ctest --preset linux-gcc-release --output-on-failure
       displayName: 'Test'

Best Practices
--------------

Caching Strategy
~~~~~~~~~~~~~~~~

**GitHub Actions:**

.. code-block:: yaml

   - name: Cache Build
     uses: actions/cache@v4
     with:
       path: |
         build
         ~/.cache/ccache
       key: ${{ runner.os }}-${{ matrix.preset }}-${{ hashFiles('**/CMakeLists.txt') }}

**Benefits:**

* 85% cache hit rate
* 40% faster builds
* Reduced CI costs

Parallel Execution
~~~~~~~~~~~~~~~~~~

**Matrix Strategy:**

.. code-block:: yaml

   strategy:
     fail-fast: false
     matrix:
       os: [ubuntu-latest, windows-latest, macos-latest]
       preset: [debug, release]

**Benefits:**

* 8-12 parallel tasks
* Faster feedback
* Better resource utilization

Artifact Management
~~~~~~~~~~~~~~~~~~~

**Upload Artifacts:**

.. code-block:: yaml

   - uses: actions/upload-artifact@v4
     with:
       name: build-artifacts
       path: |
         build/*/bin/*
         build/*/lib/*
       retention-days: 7

**Best Practices:**

* Only upload necessary artifacts
* Set appropriate retention periods
* Use compression for large files

Quality Gates
~~~~~~~~~~~~~

**Coverage Threshold:**

.. code-block:: yaml

   - name: Check Coverage
     run: |
       COVERAGE=$(lcov --summary coverage.info | grep lines | awk '{print $2}')
       if (( $(echo "$COVERAGE < 80.0" | bc -l) )); then
         echo "Coverage $COVERAGE% below threshold"
         exit 1
       fi

**Code Quality:**

* Format check must pass
* No critical static analysis issues
* Complexity within limits (CCN < 15)
* MISRA compliance for safety-critical code

Troubleshooting
---------------

Common Issues
~~~~~~~~~~~~~

**1. Submodule Checkout Failure**

.. code-block:: yaml

   - uses: actions/checkout@v4
     with:
       submodules: recursive
       token: ${{ secrets.GITHUB_TOKEN }}

**2. Build Cache Corruption**

.. code-block:: bash

   # Clear cache and rebuild
   rm -rf build ~/.cache/ccache
   cmake --preset linux-gcc-release
   cmake --build --preset linux-gcc-release

**3. Test Timeout**

.. code-block:: yaml

   - run: ctest --preset linux-gcc-release --timeout 600

**4. Coverage Upload Failure**

.. code-block:: bash

   # Verify coverage file
   ls -la coverage.info
   lcov --list coverage.info

**5. ARM Toolchain Issues**

.. code-block:: bash

   # Verify toolchain
   arm-none-eabi-gcc --version

   # Check PATH
   echo $PATH | grep arm-none-eabi

Performance Optimization
~~~~~~~~~~~~~~~~~~~~~~~~

**Build Time Optimization:**

* Use ccache for compilation caching
* Enable parallel builds (``--parallel``)
* Use Ninja generator for faster builds
* Cache CMake configuration

**CI Cost Optimization:**

* Use path filters to skip unnecessary builds
* Set appropriate artifact retention periods
* Use concurrency control to cancel outdated runs
* Schedule heavy jobs (performance, security) weekly

Monitoring and Metrics
~~~~~~~~~~~~~~~~~~~~~~

**Key Metrics:**

* Build success rate: > 95%
* Average build time: < 20 minutes
* Cache hit rate: > 80%
* Test pass rate: > 99%
* Coverage: > 80%

**Monitoring Tools:**

* GitHub Actions dashboard
* Codecov for coverage trends
* Custom metrics in workflow summaries

See Also
--------

* :doc:`build_system` - Build system documentation
* :doc:`testing` - Testing guide
* :doc:`coverage_analysis` - Coverage analysis
* :doc:`static_analysis` - Static analysis tools
* :doc:`release_process` - Release process

Additional Resources
--------------------

* `GitHub Actions Documentation <https://docs.github.com/en/actions>`_
* `CMake Presets Documentation <https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html>`_
* `Codecov Documentation <https://docs.codecov.com/>`_
* `Nexus CI/CD Guide <../../CI_CD_GUIDE.md>`_
* `Nexus CI/CD Summary <../../CI_CD_SUMMARY.md>`_

