Coverage Analysis Workflow
==========================

This guide covers the complete workflow for code coverage analysis in the Nexus Embedded Platform, including setup, execution, reporting, and integration with CI/CD systems.

.. contents:: Table of Contents
   :local:
   :depth: 2

Overview
--------

Code coverage analysis measures which parts of your code are executed during testing. The Nexus platform uses:

* **gcov**: GNU coverage tool for C/C++ code
* **lcov**: Frontend for gcov with HTML report generation
* **Hypothesis**: Property-based testing with coverage tracking

Coverage Metrics
~~~~~~~~~~~~~~~~

The system tracks three types of coverage:

* **Line Coverage**: Percentage of code lines executed
* **Branch Coverage**: Percentage of conditional branches taken
* **Function Coverage**: Percentage of functions called

Setup
-----

Prerequisites
~~~~~~~~~~~~~

**Linux/macOS:**

.. code-block:: bash

   # Ubuntu/Debian
   sudo apt-get install -y lcov

   # macOS
   brew install lcov

**Windows:**

.. code-block:: powershell

   # OpenCppCoverage (optional)
   choco install opencppcoverage

Python Dependencies
~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   pip install -r scripts/validation/requirements.txt

The main dependencies are:

* ``hypothesis``: Property-based testing
* ``pytest``: Test framework
* ``pytest-cov``: Coverage plugin for pytest

CMake Configuration
~~~~~~~~~~~~~~~~~~~

Enable coverage in CMake:

.. code-block:: bash

   CMake -B build \
       -DCMAKE_BUILD_TYPE=Debug \
       -DNEXUS_BUILD_TESTS=ON \
       -DNEXUS_ENABLE_COVERAGE=ON

This adds the necessary compiler flags:

* GCC/Clang: ``-fprofile-arcs -ftest-coverage``
* MSVC: ``/PROFILE`` (if using OpenCppCoverage)

Basic Workflow
--------------

Step 1: Build with Coverage
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Configure with coverage enabled
   CMake -B build \
       -DCMAKE_BUILD_TYPE=Debug \
       -DNEXUS_BUILD_TESTS=ON \
       -DNEXUS_ENABLE_COVERAGE=ON

   # Build
   CMake --build build

Step 2: Run Tests
~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Run all tests
   cd build
   ctest --output-on-failure

This generates ``.gcda`` and ``.gcno`` files containing coverage data.

Step 3: Collect Coverage Data
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Capture coverage data
   lcov --capture \
       --directory build \
       --output-file build/coverage.info \
       --rc lcov_branch_coverage=1

Step 4: Filter Coverage Data
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Remove external libraries and test files
   lcov --remove build/coverage.info \
       '*/ext/*' \
       '*/tests/*' \
       '*/build/*' \
       '/usr/*' \
       --output-file build/coverage_filtered.info \
       --rc lcov_branch_coverage=1

Step 5: Generate HTML Report
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Generate HTML report
   genhtml build/coverage_filtered.info \
       --output-directory build/coverage_html \
       --title "Nexus Coverage Report" \
       --legend \
       --branch-coverage \
       --rc lcov_branch_coverage=1

   # View report
   open build/coverage_html/index.html

Automated Workflow
------------------

Using Validation Framework
~~~~~~~~~~~~~~~~~~~~~~~~~~

The simplest way to run coverage analysis:

.. code-block:: bash

   python scripts/validation/validate.py --coverage

This automatically:

1. Configures CMake with coverage enabled
2. Builds the project
3. Runs all tests
4. Collects coverage data
5. Generates HTML and XML reports
6. Checks coverage threshold

With Custom Threshold
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   python scripts/validation/validate.py \
       --coverage \
       --threshold 0.85

This fails if coverage is below 85%.

Parallel Execution
~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   python scripts/validation/validate.py \
       --coverage \
       --parallel 8

Runs tests in parallel for faster execution.

Advanced Usage
--------------

Module-Specific Coverage
~~~~~~~~~~~~~~~~~~~~~~~~

**Analyze coverage for a specific module:**

.. code-block:: bash

   # Run tests for config module only
   cd build
   ctest -R "config_.*" --verbose

   # Capture coverage
   lcov --capture \
       --directory . \
       --output-file config_coverage.info

   # Extract config module only
   lcov --extract config_coverage.info \
       "*/framework/config/*" \
       --output-file config_filtered.info

   # Generate report
   genhtml config_filtered.info \
       --output-directory config_coverage_html

Incremental Coverage
~~~~~~~~~~~~~~~~~~~~

**Track coverage changes between commits:**

.. code-block:: bash

   # Baseline coverage
   python scripts/validation/validate.py --coverage
   cp build/coverage.info baseline_coverage.info

   # Make changes and test again
   # ... make code changes ...
   python scripts/validation/validate.py --coverage

   # Compare coverage
   lcov --diff baseline_coverage.info build/coverage.info \
       --output-file coverage_diff.info

   # Generate diff report
   genhtml coverage_diff.info \
       --output-directory coverage_diff_html

Coverage by Test Type
~~~~~~~~~~~~~~~~~~~~~

**Separate coverage for unit tests and property tests:**

.. code-block:: bash

   # Unit tests only
   cd build
   ctest -R "test_.*" --verbose
   lcov --capture --directory . --output-file unit_coverage.info

   # Property tests only
   ctest -R ".*_properties" --verbose
   lcov --capture --directory . --output-file property_coverage.info

   # Compare
   lcov --diff unit_coverage.info property_coverage.info

Coverage Hotspots
~~~~~~~~~~~~~~~~~

**Identify files with low coverage:**

.. code-block:: bash

   # Generate coverage summary
   lcov --summary build/coverage.info

   # List files by coverage
   lcov --list build/coverage.info | sort -k 2 -n

   # Find files below 80% coverage
   lcov --list build/coverage.info | awk '$2 < 80.0 {print $0}'

Report Formats
--------------

HTML Report
~~~~~~~~~~~

The HTML report provides:

* **File-level statistics**: Coverage percentage per file
* **Source code view**: Line-by-line coverage visualization
* **Branch coverage**: Conditional branch analysis
* **Function coverage**: Function call tracking

**Color coding:**

* 🟢 **Green**: Covered lines (executed)
* 🔴 **Red**: Uncovered lines (not executed)
* 🟡 **Yellow**: Partially covered branches

XML Report (Cobertura)
~~~~~~~~~~~~~~~~~~~~~~

For CI/CD integration:

.. code-block:: bash

   # Install lcov_cobertura
   pip install lcov_cobertura

   # Convert to Cobertura XML
   python -m lcov_cobertura build/coverage.info \
       --output build/coverage.xml

JSON Report
~~~~~~~~~~~

For programmatic analysis:

.. code-block:: bash

   # Generate JSON summary
   lcov --summary build/coverage.info --output-file coverage_summary.json

Text Summary
~~~~~~~~~~~~

For quick overview:

.. code-block:: bash

   # Print summary to console
   lcov --summary build/coverage.info

   # Example output:
   # Overall coverage rate:
   #   lines......: 85.2% (1234 of 1448 lines)
   #   functions..: 90.1% (123 of 137 functions)
   #   branches...: 78.5% (456 of 581 branches)

Coverage Thresholds
-------------------

Setting Thresholds
~~~~~~~~~~~~~~~~~~

**Project-wide threshold:**

.. code-block:: bash

   python scripts/validation/validate.py \
       --coverage \
       --threshold 0.80

**Module-specific thresholds:**

.. code-block:: python

   from scripts.validation.coverage_analyzer import CoverageAnalyzer
   from scripts.validation.config import ConfigManager

   config = ConfigManager().load_default()
   config.coverage_enabled = True

   analyzer = CoverageAnalyzer(config)
   coverage_data = analyzer.collect_coverage_data()

   # Check different thresholds for different modules
   thresholds = {
       'framework/config': 0.90,
       'framework/log': 0.85,
       'hal': 0.80,
       'osal': 0.80
   }

   for module, threshold in thresholds.items():
       module_coverage = get_module_coverage(coverage_data, module)
       assert module_coverage >= threshold, \
           f"{module} coverage {module_coverage} below threshold {threshold}"

Enforcing Thresholds
~~~~~~~~~~~~~~~~~~~~

**In CI/CD:**

.. code-block:: yaml

   # GitHub Actions
   - name: Check Coverage
     run: |
       python scripts/validation/validate.py --coverage --threshold 0.80
       if [ $? -ne 0 ]; then
         echo "Coverage below threshold"
         exit 1
       fi

**Pre-commit hook:**

.. code-block:: bash

   #!/bin/bash
   # .git/hooks/pre-commit

   # Run coverage check
   python scripts/validation/validate.py --coverage --threshold 0.80

   if [ $? -ne 0 ]; then
       echo "Coverage check failed. Commit rejected."
       exit 1
   fi

CI/CD Integration
-----------------

GitHub Actions
~~~~~~~~~~~~~~

.. code-block:: yaml

   name: Coverage
   on: [push, pull_request]

   jobs:
     coverage:
       runs-on: ubuntu-latest
       steps:
         - uses: actions/checkout@v3
           with:
             submodules: recursive

         - name: Install dependencies
           run: |
             sudo apt-get install -y lcov
             pip install -r scripts/validation/requirements.txt

         - name: Run coverage
           run: python scripts/validation/validate.py --coverage

         - name: Upload to Codecov
           uses: codecov/codecov-action@v3
           with:
             files: ./build/coverage.info
             flags: unittests
             name: codecov-nexus

         - name: Upload coverage report
           uses: actions/upload-artifact@v3
           with:
             name: coverage-report
             path: validation_reports/coverage_html/

GitLab CI
~~~~~~~~~

.. code-block:: yaml

   coverage:
     stage: test
     script:
       - pip install -r scripts/validation/requirements.txt
       - python scripts/validation/validate.py --coverage
     coverage: '/line_coverage: (\d+\.\d+)%/'
     artifacts:
       reports:
         coverage_report:
           coverage_format: cobertura
           path: build/coverage.xml

Jenkins
~~~~~~~

.. code-block:: groovy

   stage('Coverage') {
       steps {
           sh 'python scripts/validation/validate.py --coverage'
           publishHTML([
               reportDir: 'validation_reports/coverage_html',
               reportFiles: 'index.html',
               reportName: 'Coverage Report'
           ])
           publishCoverage adapters: [
               coberturaAdapter('build/coverage.xml')
           ]
       }
   }

Codecov Integration
~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Install Codecov uploader
   curl -Os https://uploader.codecov.io/latest/linux/codecov
   chmod +x codecov

   # Upload coverage
   ./codecov -f build/coverage.info -t ${CODECOV_TOKEN}

Troubleshooting
---------------

No Coverage Data
~~~~~~~~~~~~~~~~

**Problem:** No ``.gcda`` files generated

**Solution:**

.. code-block:: bash

   # Verify coverage flags are set
   CMake -B build -DNEXUS_ENABLE_COVERAGE=ON
   CMake -L build | grep COVERAGE

   # Check compiler flags
   CMake --build build --verbose | grep "fprofile-arcs"

   # Ensure tests actually run
   cd build && ctest --verbose

Low Coverage Numbers
~~~~~~~~~~~~~~~~~~~~

**Problem:** Coverage lower than expected

**Solution:**

.. code-block:: bash

   # Check which files are included
   lcov --list build/coverage.info

   # Verify test execution
   ctest --verbose --output-on-failure

   # Check for excluded files
   lcov --list build/coverage.info | grep "excluded"

Coverage Data Corruption
~~~~~~~~~~~~~~~~~~~~~~~~

**Problem:** Invalid coverage data

**Solution:**

.. code-block:: bash

   # Clean all coverage data
   find build -name "*.gcda" -delete
   find build -name "*.gcno" -delete

   # Rebuild and retest
   CMake --build build --clean-first
   cd build && ctest

Missing Branch Coverage
~~~~~~~~~~~~~~~~~~~~~~~

**Problem:** Branch coverage not reported

**Solution:**

.. code-block:: bash

   # Ensure branch coverage is enabled
   lcov --capture \
       --directory build \
       --output-file coverage.info \
       --rc lcov_branch_coverage=1

   # Generate report with branch coverage
   genhtml coverage.info \
       --output-directory coverage_html \
       --branch-coverage \
       --rc lcov_branch_coverage=1

Best Practices
--------------

Regular Coverage Checks
~~~~~~~~~~~~~~~~~~~~~~~

* Run coverage analysis on every commit
* Set up pre-commit hooks for local checks
* Configure CI/CD to fail on coverage drops

Coverage Goals
~~~~~~~~~~~~~~

* **Minimum threshold**: 80% overall coverage
* **Critical modules**: 90%+ coverage (config, HAL core)
* **New code**: 100% coverage for new features

Focus on Quality
~~~~~~~~~~~~~~~~

* High coverage doesn't guarantee quality
* Focus on meaningful tests, not just coverage numbers
* Use property-based testing for better coverage

Incremental Improvement
~~~~~~~~~~~~~~~~~~~~~~~

* Track coverage trends over time
* Set incremental goals (e.g., +5% per quarter)
* Prioritize uncovered critical paths

Documentation
~~~~~~~~~~~~~

* Document coverage requirements in README
* Include coverage badges in documentation
* Explain coverage gaps and plans to address them

API Reference
-------------

CoverageAnalyzer Class
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: python

   from scripts.validation.coverage_analyzer import CoverageAnalyzer
   from scripts.validation.config import ConfigManager

   # Create analyzer
   config = ConfigManager().load_default()
   config.coverage_enabled = True
   analyzer = CoverageAnalyzer(config)

   # Enable coverage
   analyzer.enable_coverage()

   # Collect data
   coverage_data = analyzer.collect_coverage_data()

   # Generate reports
   html_report = analyzer.generate_coverage_report(format="html")
   xml_report = analyzer.generate_coverage_report(format="xml")

   # Check threshold
   meets_threshold = analyzer.check_threshold(0.80)

   # Get uncovered regions
   uncovered = analyzer.identify_uncovered_regions()

   # Get summary
   summary = analyzer.get_coverage_summary()

CoverageData Model
~~~~~~~~~~~~~~~~~~

.. code-block:: python

   @dataclass
   class CoverageData:
       line_coverage: float          # 0.0 to 1.0
       branch_coverage: float        # 0.0 to 1.0
       function_coverage: float      # 0.0 to 1.0
       uncovered_lines: List[CodeLocation]
       uncovered_branches: List[CodeLocation]

CodeLocation Model
~~~~~~~~~~~~~~~~~~

.. code-block:: python

   @dataclass
   class CodeLocation:
       file_path: str
       line_number: int

See Also
--------

* :doc:`validation_framework` - System validation framework
* :doc:`testing` - General testing guide
* :doc:`ci_cd_integration` - CI/CD integration
* :doc:`contributing` - Contribution guidelines
