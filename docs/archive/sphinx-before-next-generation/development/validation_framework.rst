Validation Framework
====================

The Nexus Validation Framework is a comprehensive automated testing and coverage analysis tool for verifying the correctness of the Nexus Embedded Platform's configuration system, HAL framework, OSAL framework, and platform implementations.

.. contents:: Table of Contents
   :local:
   :depth: 2

Overview
--------

The validation framework provides:

* **Test Execution**: Unit tests, property-based tests, and integration tests
* **Coverage Analysis**: Line, branch, and function coverage tracking
* **Report Generation**: HTML, XML, and JUnit format reports
* **CI/CD Integration**: GitHub Actions, GitLab CI, and Jenkins support

System Requirements
-------------------

Required Tools
~~~~~~~~~~~~~~

* **Python**: 3.8 or higher
* **CMake**: 3.15 or higher
* **Ninja**: Build system (recommended)
* **C/C++ Compiler**: GCC 9+, Clang 10+, or MSVC 2019+

Coverage Tools
~~~~~~~~~~~~~~

* **Linux/macOS**: gcov + lcov
* **Windows**: OpenCppCoverage (optional)

Python Dependencies
~~~~~~~~~~~~~~~~~~~

Install required Python packages:

.. code-block:: bash

   pip install -r scripts/validation/requirements.txt

Main dependencies:

* ``hypothesis``: Property-based testing framework
* ``pytest``: Python testing framework
* ``jinja2``: Report template engine
* ``lxml``: XML processing
* ``pyyaml``: Configuration file parsing

Installation
------------

1. Clone Repository
~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   git clone https://github.com/your-org/nexus.git
   cd nexus
   git submodule update --init --recursive

2. Install System Dependencies
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Ubuntu/Debian:**

.. code-block:: bash

   sudo apt-get update
   sudo apt-get install -y CMake ninja-build lcov python3-pip

**macOS:**

.. code-block:: bash

   brew install CMake ninja lcov python3

**Windows:**

.. code-block:: powershell

   choco install CMake ninja python3

3. Install Python Dependencies
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   pip install -r scripts/validation/requirements.txt

4. Verify Installation
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   python scripts/validation/validate.py --help

Quick Start
-----------

Basic Usage
~~~~~~~~~~~

**Run all tests:**

.. code-block:: bash

   python scripts/validation/validate.py

**Enable coverage analysis:**

.. code-block:: bash

   python scripts/validation/validate.py --coverage

**Set coverage threshold:**

.. code-block:: bash

   python scripts/validation/validate.py --coverage --threshold 0.85

**Parallel test execution:**

.. code-block:: bash

   python scripts/validation/validate.py --parallel 4

Typical Workflow
~~~~~~~~~~~~~~~~

.. code-block:: bash

   # 1. Clean previous builds
   rm -rf build validation_reports

   # 2. Run full validation with coverage
   python scripts/validation/validate.py \
       --coverage \
       --threshold 0.80 \
       --parallel 8 \
       --report-dir validation_reports

   # 3. View reports
   # HTML report: validation_reports/report.html
   # Coverage report: validation_reports/coverage/index.html
   # JUnit XML: validation_reports/junit.xml

Command-Line Options
--------------------

Basic Options
~~~~~~~~~~~~~

.. code-block:: text

   --build-dir DIR       Build directory (default: build)
   --source-dir DIR      Source code directory (default: .)

Coverage Options
~~~~~~~~~~~~~~~~

.. code-block:: text

   --coverage            Enable code coverage analysis
   --threshold FLOAT     Coverage threshold 0.0-1.0 (default: 0.80)

Test Execution Options
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: text

   --timeout SECONDS     Test timeout in seconds (default: 300)
   --parallel N          Number of parallel jobs (default: auto-detect)
   --fail-fast           Stop on first test failure

Report Options
~~~~~~~~~~~~~~

.. code-block:: text

   --report-dir DIR      Report output directory (default: build/validation_reports)

Configuration File Options
~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: text

   --config FILE         Load configuration from JSON file
   --save-config FILE    Save current configuration to JSON file

Output Options
~~~~~~~~~~~~~~

.. code-block:: text

   --verbose, -v         Enable verbose output mode
   --quiet, -q           Enable quiet mode (minimal output)

Coverage Analysis
-----------------

Generating Coverage Reports
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   python scripts/validation/validate.py --coverage

Coverage reports are generated in the ``validation_reports/coverage/`` directory:

* ``index.html``: Main coverage report
* ``coverage.info``: lcov format data
* ``coverage.xml``: Cobertura XML format

Checking Coverage Threshold
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   python scripts/validation/check_coverage.py \
       --coverage-file build/coverage.info \
       --threshold 0.80

Viewing Uncovered Regions
~~~~~~~~~~~~~~~~~~~~~~~~~

The coverage report highlights:

* 🔴 **Red**: Unexecuted code lines
* 🟡 **Yellow**: Partially covered branches
* 🟢 **Green**: Fully covered code

Coverage Analysis Examples
~~~~~~~~~~~~~~~~~~~~~~~~~~

**Example 1: Basic Coverage Analysis**

.. code-block:: bash

   # Run tests with coverage
   python scripts/validation/validate.py --coverage

   # View HTML report
   open validation_reports/coverage_html/index.html

**Example 2: Coverage with Threshold Check**

.. code-block:: bash

   # Run with 85% threshold
   python scripts/validation/validate.py --coverage --threshold 0.85

   # Exit code 0 if threshold met, non-zero otherwise
   echo $?

**Example 3: Coverage for Specific Module**

.. code-block:: bash

   # Build and run tests for config module only
   cd build
   ctest -R "config_.*" --verbose

   # Generate coverage for config module
   lcov --capture --directory . --output-file config_coverage.info
   lcov --extract config_coverage.info "*/framework/config/*" --output-file config_filtered.info
   genhtml config_filtered.info --output-directory config_coverage_html

**Example 4: Identifying Uncovered Code**

.. code-block:: python

   # Using the coverage analyzer API
   from scripts.validation.coverage_analyzer import CoverageAnalyzer
   from scripts.validation.config import ConfigManager

   config = ConfigManager().load_default()
   config.coverage_enabled = True

   analyzer = CoverageAnalyzer(config)
   coverage_data = analyzer.collect_coverage_data()

   # Get uncovered regions
   uncovered = analyzer.identify_uncovered_regions()

   for location in uncovered[:10]:
       print(f"{location.file_path}:{location.line_number}")

**Example 5: Coverage Report Formats**

.. code-block:: bash

   # Generate HTML report
   python scripts/validation/validate.py --coverage
   genhtml build/coverage.info --output-directory coverage_html

   # Generate XML report for CI
   python -m lcov_cobertura build/coverage.info --output coverage.xml

   # Generate JSON report
   lcov --summary build/coverage.info --output-file coverage_summary.json

Property-Based Testing
----------------------

Property-based tests use the Hypothesis library and run at least 100 iterations per test:

.. code-block:: python

   from hypothesis import given, settings
   import hypothesis.strategies as st

   @settings(max_examples=100)
   @given(value=st.integers())
   def test_property(value):
       """
       Feature: system-validation, Property 1: Test property
       Validates: Requirements 1.1
       """
       assert some_property_holds(value)

Viewing Failing Examples
~~~~~~~~~~~~~~~~~~~~~~~~

When a property test fails, the counterexample is displayed:

.. code-block:: text

   Falsifying example: test_property(value=42)

Configuration
-------------

Configuration File
~~~~~~~~~~~~~~~~~~

Create ``validation_config.yaml`` for custom configuration:

.. code-block:: yaml

   # Build configuration
   build:
     directory: build
     generator: Ninja
     build_type: Debug

   # Test configuration
   testing:
     parallel_jobs: 8
     timeout: 300
     fail_fast: false

   # Coverage configuration
   coverage:
     enabled: true
     threshold: 0.80
     exclude_patterns:
       - "*/tests/*"
       - "*/ext/*"

   # Report configuration
   reporting:
     output_dir: validation_reports
     formats:
       - html
       - junit
       - json

Using the configuration file:

.. code-block:: bash

   python scripts/validation/validate.py --config validation_config.yaml

Environment Variables
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # Set build directory
   export NEXUS_BUILD_DIR=build_coverage

   # Set coverage threshold
   export NEXUS_COVERAGE_THRESHOLD=0.85

   # Set parallel jobs
   export NEXUS_PARALLEL_JOBS=16

   # Run validation
   python scripts/validation/validate.py

Report Formats
--------------

HTML Report
~~~~~~~~~~~

The main report (``validation_reports/report.html``) contains:

* Test results summary table
* Coverage statistics charts
* Failed test details
* Performance analysis
* Interactive filtering and search

JUnit XML Report
~~~~~~~~~~~~~~~~

JUnit format (``validation_reports/junit.xml``) for CI integration:

.. code-block:: xml

   <testsuites>
     <testsuite name="config_tests" tests="10" failures="0" time="1.23">
       <testcase name="test_config_set" time="0.12"/>
       ...
     </testsuite>
   </testsuites>

Coverage Report
~~~~~~~~~~~~~~~

Coverage report (``validation_reports/coverage/index.html``) provides:

* File-level coverage statistics
* Source code line-level coverage visualization
* List of uncovered code regions
* Branch coverage details

CI/CD Integration
-----------------

GitHub Actions
~~~~~~~~~~~~~~

The project includes a pre-configured GitHub Actions workflow (``.github/workflows/validation.yml``):

.. code-block:: yaml

   name: System Validation
   on: [push, pull_request]
   jobs:
     validate:
       runs-on: ubuntu-latest
       steps:
         - uses: actions/checkout@v3
         - name: Run Validation
           run: python scripts/validation/validate.py --coverage
         - name: Upload Coverage
           uses: codecov/codecov-action@v3

Local CI Simulation
~~~~~~~~~~~~~~~~~~~

Simulate CI environment locally:

.. code-block:: bash

   # Clean environment
   rm -rf build validation_reports

   # Run validation (CI mode)
   python scripts/validation/validate.py \
       --coverage \
       --threshold 0.80 \
       --fail-fast \
       --report-dir validation_reports

   # Check exit code
   echo $?  # 0 = success, non-zero = failure

GitLab CI
~~~~~~~~~

.. code-block:: yaml

   test:
     script:
       - pip install -r scripts/validation/requirements.txt
       - python scripts/validation/validate.py --coverage
     artifacts:
       reports:
         junit: validation_reports/junit.xml
         coverage_report:
           coverage_format: cobertura
           path: validation_reports/coverage.xml

Jenkins
~~~~~~~

.. code-block:: groovy

   stage('Validation') {
       steps {
           sh 'python scripts/validation/validate.py --coverage'
           junit 'validation_reports/junit.xml'
           publishHTML([
               reportDir: 'validation_reports',
               reportFiles: 'report.html',
               reportName: 'Validation Report'
           ])
       }
   }

Troubleshooting
---------------

Common Issues
~~~~~~~~~~~~~

**1. Build Failure**

.. code-block:: text

   Error: CMake configuration failed

**Solution:**

* Check CMake version >= 3.15
* Ensure all submodules are initialized: ``git submodule update --init --recursive``
* Clean build directory: ``rm -rf build``

**2. Test Timeout**

.. code-block:: text

   Error: Test timeout after 300 seconds

**Solution:**

* Increase timeout: ``--test-timeout 600``
* Check for deadlocks or infinite loops
* Reduce parallel jobs: ``--parallel 2``

**3. Missing Coverage Data**

.. code-block:: text

   Warning: No coverage data found

**Solution:**

* Ensure ``--coverage`` option is used
* Check compiler supports coverage: ``gcc --version``
* Verify lcov is installed: ``lcov --version``

**4. Property Test Failure**

.. code-block:: text

   Falsifying example: test_property(value=42)

**Solution:**

* Check if counterexample reveals a real bug
* If test issue, adjust test strategy
* If code issue, fix implementation

Debugging Tips
~~~~~~~~~~~~~~

**Enable verbose output:**

.. code-block:: bash

   python scripts/validation/validate.py --verbose

**View CMake configuration:**

.. code-block:: bash

   CMake -B build -DNEXUS_BUILD_TESTS=ON -DNEXUS_ENABLE_COVERAGE=ON
   CMake -L build  # List all configuration options

**Manually run single test:**

.. code-block:: bash

   cd build
   ./tests/config/config_tests --gtest_filter="ConfigTest.SetGet"

**Check coverage data:**

.. code-block:: bash

   # View raw coverage data
   lcov --list build/coverage.info

   # Generate coverage report
   genhtml build/coverage.info --output-directory coverage_html

API Reference
-------------

ValidationController
~~~~~~~~~~~~~~~~~~~~

Main controller for running validation:

.. code-block:: python

   from scripts.validation.validation_controller import ValidationController
   from scripts.validation.config import ConfigManager

   config = ConfigManager().load_default()
   controller = ValidationController(config)
   result = controller.run_all_tests()

CoverageAnalyzer
~~~~~~~~~~~~~~~~

Coverage analysis and reporting:

.. code-block:: python

   from scripts.validation.coverage_analyzer import CoverageAnalyzer

   analyzer = CoverageAnalyzer(config)
   analyzer.enable_coverage()
   coverage_data = analyzer.collect_coverage_data()
   analyzer.generate_coverage_report(format="html")
   meets_threshold = analyzer.check_threshold(0.80)

ReportGenerator
~~~~~~~~~~~~~~~

Generate validation reports:

.. code-block:: python

   from scripts.validation.report_generator import ReportGenerator

   generator = ReportGenerator(config)
   generator.generate_html_report(validation_result)
   generator.generate_junit_report(validation_result)

See Also
--------

* :doc:`testing` - General testing guide
* :doc:`scripts` - Build scripts documentation
* :doc:`contributing` - Contribution guidelines
