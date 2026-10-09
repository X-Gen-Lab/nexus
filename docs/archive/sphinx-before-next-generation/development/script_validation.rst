Script Validation System
========================

The Script Validation System is a comprehensive cross-platform script verification framework designed specifically for the Nexus Embedded System project. It automates the validation of all project scripts across Windows, WSL, and Linux environments.

.. contents:: Table of Contents
   :local:
   :depth: 2

Overview
--------

The system automatically validates the functionality, compatibility, and reliability of all project scripts across multiple platforms.

Key Features
~~~~~~~~~~~~

* **Cross-Platform Validation**: Supports Windows, WSL, and Linux platforms
* **Multiple Validators**: Functional, compatibility, performance, and documentation validation
* **Multiple Report Formats**: HTML, JSON, Summary, and JUnit XML
* **CI/CD Integration**: Supports GitHub Actions, GitLab CI, Jenkins, Azure DevOps
* **Flexible Configuration**: Command-line arguments and configuration files

Architecture
------------

Directory Structure
~~~~~~~~~~~~~~~~~~~

.. code-block:: text

   script_validation/
   ├── adapters/           # Platform adapters (Windows/WSL/Linux)
   ├── controllers/        # Validation controllers
   ├── handlers/           # Error handling and resource management
   ├── managers/           # Script and platform managers
   ├── reporters/          # Report generators (HTML/JSON/JUnit/Summary)
   ├── validators/         # Validators (functional/compatibility/performance/documentation)
   ├── __init__.py         # Module entry point
   ├── __main__.py         # CLI entry point
   ├── ci_integration.py   # CI/CD integration
   ├── discovery.py        # Script discovery
   ├── integration.py      # Component integration
   ├── interfaces.py       # Interface definitions
   └── models.py           # Data models

Components
~~~~~~~~~~

**Platform Adapters**
   Handle platform-specific script execution and environment setup

**Validators**
   Perform different types of validation:

   * Functional: Verify script functionality
   * Compatibility: Check cross-platform compatibility
   * Performance: Measure execution performance
   * Documentation: Validate documentation completeness

**Reporters**
   Generate validation reports in various formats

**Controllers**
   Orchestrate the validation workflow

Quick Start
-----------

Command-Line Usage
~~~~~~~~~~~~~~~~~~

**Full validation:**

.. code-block:: bash

   python -m script_validation --mode full

**Quick validation:**

.. code-block:: bash

   python -m script_validation --mode quick

**Platform-specific validation:**

.. code-block:: bash

   python -m script_validation --platforms windows wsl

**Generate specific format reports:**

.. code-block:: bash

   python -m script_validation --report-format html json

**CI mode:**

.. code-block:: bash

   python -m script_validation --ci

**Generate JUnit XML report:**

.. code-block:: bash

   python -m script_validation --ci --report-format junit

**List discovered scripts:**

.. code-block:: bash

   python -m script_validation --list-scripts

**Check platform availability:**

.. code-block:: bash

   python -m script_validation --check-platforms

Programming Interface
~~~~~~~~~~~~~~~~~~~~~

**Method 1: Using convenience functions:**

.. code-block:: python

   from script_validation import run_validation

   report = run_validation(mode='full')

**Method 2: Using workflow:**

.. code-block:: python

   from script_validation import create_workflow, Platform

   workflow = create_workflow(
       platforms=[Platform.WINDOWS, Platform.WSL],
       validators=['functional', 'compatibility'],
       report_formats=['html', 'json']
   )
   report = workflow.run()

**Method 3: Using builder pattern:**

.. code-block:: python

   from script_validation import ValidationBuilder, Platform
   from pathlib import Path

   workflow = (ValidationBuilder()
       .root_path(Path('./'))
       .platforms(Platform.WINDOWS, Platform.LINUX)
       .validators('functional', 'performance')
       .report_formats('html', 'junit')
       .timeout(600)
       .ci_mode(True)
       .build())
   report = workflow.run()

Command-Line Arguments
----------------------

.. list-table::
   :header-rows: 1
   :widths: 20 10 40 30

   * - Argument
     - Short
     - Description
     - Default
   * - ``--root-path``
     - ``-r``
     - Project root directory path
     - Current directory
   * - ``--mode``
     - ``-m``
     - Validation mode: full/quick/platform-specific
     - full
   * - ``--platforms``
     - ``-p``
     - Target platforms: windows/wsl/linux
     - All available
   * - ``--report-format``
     - ``-f``
     - Report format: html/json/summary/junit/all
     - all
   * - ``--output-dir``
     - ``-o``
     - Report output directory
     - ./validation_reports
   * - ``--validators``
     - ``-v``
     - Validators: functional/compatibility/performance/documentation
     - All
   * - ``--timeout``
     -
     - Script execution timeout (seconds)
     - 300
   * - ``--max-memory``
     -
     - Maximum memory limit (MB)
     - 1024
   * - ``--ci``
     -
     - CI mode
     - false
   * - ``--verbose``
     -
     - Verbose output
     - false
   * - ``--no-parallel``
     -
     - Disable parallel execution
     - false

Cross-Platform Testing Guide
----------------------------

Platform Support
~~~~~~~~~~~~~~~~

The script validation system supports three platforms:

* **Windows**: Native Windows environment
* **WSL**: Windows Subsystem for Linux
* **Linux**: Native Linux environment

Testing on Windows
~~~~~~~~~~~~~~~~~~

**Prerequisites:**

* Python 3.8+
* PowerShell 5.1+
* Git for Windows

**Running validation:**

.. code-block:: powershell

   # Full validation on Windows
   python -m script_validation --platforms windows

   # Test specific script types
   python -m script_validation --platforms windows --validators functional

**Common issues:**

* Path separators: Use ``Path`` objects for cross-platform compatibility
* Line endings: Ensure scripts handle both CRLF and LF
* Shell differences: Test both CMD and PowerShell execution

Testing on WSL
~~~~~~~~~~~~~~

**Prerequisites:**

* WSL 2 installed
* Python 3.8+ in WSL
* Access to Windows filesystem

**Running validation:**

.. code-block:: bash

   # Full validation on WSL
   python -m script_validation --platforms wsl

   # Test cross-platform compatibility
   python -m script_validation --platforms windows wsl --validators compatibility

**Common issues:**

* File permissions: WSL may have different permissions than Windows
* Path translation: Windows paths need conversion in WSL
* Performance: File I/O may be slower across filesystem boundaries

Testing on Linux
~~~~~~~~~~~~~~~~

**Prerequisites:**

* Python 3.8+
* Bash shell
* Standard Unix utilities

**Running validation:**

.. code-block:: bash

   # Full validation on Linux
   python -m script_validation --platforms linux

   # Performance testing
   python -m script_validation --platforms linux --validators performance

**Common issues:**

* Shell differences: Test with both bash and sh
* Dependencies: Ensure all required tools are installed
* Permissions: Check execute permissions on scripts

Cross-Platform Testing Examples
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Example 1: Full Cross-Platform Validation**

.. code-block:: bash

   # Test on all available platforms
   python -m script_validation --mode full --platforms windows wsl linux

   # View HTML report
   open validation_reports/report.html

**Example 2: Compatibility Testing**

.. code-block:: bash

   # Focus on compatibility issues
   python -m script_validation \
       --validators compatibility \
       --platforms windows linux \
       --report-format html

**Example 3: Performance Comparison**

.. code-block:: bash

   # Compare performance across platforms
   python -m script_validation \
       --validators performance \
       --platforms windows wsl linux \
       --report-format json

   # Analyze results
   python -c "
   import json
   with open('validation_reports/report.json') as f:
       data = json.load(f)
       for platform, results in data['performance'].items():
           print(f'{platform}: {results[\"avg_time\"]}s')
   "

**Example 4: CI Pipeline Testing**

.. code-block:: bash

   # Simulate CI environment
   python -m script_validation \
       --ci \
       --platforms linux \
       --report-format junit \
       --timeout 600

   # Check exit code
   echo $?

**Example 5: Selective Script Testing**

.. code-block:: python

   from script_validation import ValidationBuilder, Platform
   from pathlib import Path

   # Test only build scripts
   workflow = (ValidationBuilder()
       .root_path(Path('./scripts/building'))
       .platforms(Platform.WINDOWS, Platform.LINUX)
       .validators('functional')
       .build())

   report = workflow.run()

   if report.success:
       print("Build scripts validated successfully")
   else:
       print(f"Validation failed: {report.failures}")

CI/CD Integration
-----------------

The system automatically detects CI environments and adjusts output format:

* **GitHub Actions**: Uses ``::error::`` and ``::warning::`` annotations
* **GitLab CI**: Uses collapsible sections
* **Azure DevOps**: Uses ``##vso`` commands
* **Jenkins**: Standard output format

Exit Codes
~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Code
     - Meaning
   * - 0
     - Validation successful
   * - 1
     - Validation failed
   * - 2
     - Execution error

GitHub Actions Example
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: yaml

   name: Script Validation
   on: [push, pull_request]

   jobs:
     validate:
       runs-on: ${{ matrix.os }}
       strategy:
         matrix:
           os: [ubuntu-latest, windows-latest]

       steps:
         - uses: actions/checkout@v3

         - name: Set up Python
           uses: actions/setup-python@v4
           with:
             python-version: '3.10'

         - name: Install dependencies
           run: pip install -r requirements.txt

         - name: Validate Scripts
           run: python -m script_validation --ci --report-format junit

         - name: Upload Report
           if: always()
           uses: actions/upload-artifact@v3
           with:
             name: validation-report-${{ matrix.os }}
             path: validation_reports/

         - name: Publish Test Results
           if: always()
           uses: EnricoMi/publish-unit-test-result-action@v2
           with:
             files: validation_reports/junit.xml

GitLab CI Example
~~~~~~~~~~~~~~~~~

.. code-block:: yaml

   script_validation:
     stage: test
     parallel:
       matrix:
         - PLATFORM: [windows, linux]
     script:
       - pip install -r requirements.txt
       - python -m script_validation --ci --platforms $PLATFORM --report-format junit
     artifacts:
       reports:
         junit: validation_reports/junit.xml
       paths:
         - validation_reports/
       when: always

Jenkins Example
~~~~~~~~~~~~~~~

.. code-block:: groovy

   pipeline {
       agent any

       stages {
           stage('Validate Scripts') {
               parallel {
                   stage('Windows') {
                       agent { label 'windows' }
                       steps {
                           bat 'python -m script_validation --ci --platforms windows --report-format junit'
                       }
                   }
                   stage('Linux') {
                       agent { label 'linux' }
                       steps {
                           sh 'python -m script_validation --ci --platforms linux --report-format junit'
                       }
                   }
               }
           }
       }

       post {
           always {
               junit 'validation_reports/junit.xml'
               publishHTML([
                   reportDir: 'validation_reports',
                   reportFiles: 'report.html',
                   reportName: 'Script Validation Report'
               ])
           }
       }
   }

Azure DevOps Example
~~~~~~~~~~~~~~~~~~~~

.. code-block:: yaml

   trigger:
     - main

   pool:
     vmImage: 'ubuntu-latest'

   steps:
     - task: UsePythonVersion@0
       inputs:
         versionSpec: '3.10'

     - script: |
         pip install -r requirements.txt
         python -m script_validation --ci --report-format junit
       displayName: 'Validate Scripts'

     - task: PublishTestResults@2
       condition: always()
       inputs:
         testResultsFormat: 'JUnit'
         testResultsFiles: 'validation_reports/junit.xml'

     - task: PublishBuildArtifacts@1
       condition: always()
       inputs:
         pathToPublish: 'validation_reports'
         artifactName: 'validation-report'

Validators
----------

Functional Validator
~~~~~~~~~~~~~~~~~~~~

Verifies that scripts execute correctly and produce expected results.

**Checks:**

* Script exits with code 0 on success
* Required output files are created
* Output matches expected format
* Error handling works correctly

**Example:**

.. code-block:: python

   from script_validation.validators import FunctionalValidator

   validator = FunctionalValidator()
   result = validator.validate_script(script_path, platform)

Compatibility Validator
~~~~~~~~~~~~~~~~~~~~~~~

Checks cross-platform compatibility issues.

**Checks:**

* Path separators are handled correctly
* Line endings are compatible
* Shell commands work on all platforms
* Dependencies are available

**Example:**

.. code-block:: python

   from script_validation.validators import CompatibilityValidator

   validator = CompatibilityValidator()
   result = validator.validate_script(script_path, platforms)

Performance Validator
~~~~~~~~~~~~~~~~~~~~~

Measures script execution performance.

**Metrics:**

* Execution time
* Memory usage
* CPU usage
* I/O operations

**Example:**

.. code-block:: python

   from script_validation.validators import PerformanceValidator

   validator = PerformanceValidator()
   result = validator.validate_script(script_path, platform)

Documentation Validator
~~~~~~~~~~~~~~~~~~~~~~~

Validates script documentation completeness.

**Checks:**

* Help text is available
* Usage examples are provided
* All options are documented
* Error messages are clear

**Example:**

.. code-block:: python

   from script_validation.validators import DocumentationValidator

   validator = DocumentationValidator()
   result = validator.validate_script(script_path)

Report Formats
--------------

HTML Report
~~~~~~~~~~~

Interactive HTML report with:

* Summary dashboard
* Platform-specific results
* Detailed failure information
* Performance charts
* Filtering and search

**Location:** ``validation_reports/report.html``

JSON Report
~~~~~~~~~~~

Machine-readable JSON format:

.. code-block:: json

   {
     "summary": {
       "total_scripts": 42,
       "passed": 40,
       "failed": 2,
       "platforms": ["windows", "linux"]
     },
     "results": [
       {
         "script": "build.py",
         "platform": "windows",
         "status": "passed",
         "duration": 1.23
       }
     ]
   }

**Location:** ``validation_reports/report.json``

JUnit XML Report
~~~~~~~~~~~~~~~~

Standard JUnit XML format for CI integration:

.. code-block:: xml

   <testsuites>
     <testsuite name="script_validation" tests="42" failures="2">
       <testcase name="build.py[windows]" time="1.23"/>
       <testcase name="test.py[linux]" time="2.45">
         <failure message="Script failed">...</failure>
       </testcase>
     </testsuite>
   </testsuites>

**Location:** ``validation_reports/junit.xml``

Summary Report
~~~~~~~~~~~~~~

Concise text summary:

.. code-block:: text

   Script Validation Summary
   =========================
   Total Scripts: 42
   Passed: 40
   Failed: 2
   Success Rate: 95.2%

   Failures:
   - test.py [linux]: Exit code 1
   - format.py [windows]: Timeout

**Location:** ``validation_reports/summary.txt``

Testing
-------

Running Tests
~~~~~~~~~~~~~

.. code-block:: bash

   # Run all tests
   python -m pytest tests/script_validation/ -v

   # Run property tests
   python -m pytest tests/script_validation/ -v --hypothesis-show-statistics

   # Generate coverage report
   python -m pytest tests/script_validation/ --cov=script_validation --cov-report=html

Test Structure
~~~~~~~~~~~~~~

.. code-block:: text

   tests/script_validation/
   ├── test_adapters.py           # Platform adapter tests
   ├── test_validators.py         # Validator tests
   ├── test_reporters.py          # Reporter tests
   ├── test_integration.py        # Integration tests
   └── test_properties.py         # Property-based tests

Troubleshooting
---------------

Common Issues
~~~~~~~~~~~~~

**1. Platform Not Available**

.. code-block:: text

   Error: Platform 'wsl' not available

**Solution:**

* Check platform is installed
* Verify Python is accessible on platform
* Use ``--check-platforms`` to diagnose

**2. Script Timeout**

.. code-block:: text

   Error: Script timeout after 300 seconds

**Solution:**

* Increase timeout: ``--timeout 600``
* Check for infinite loops
* Optimize script performance

**3. Permission Denied**

.. code-block:: text

   Error: Permission denied: script.sh

**Solution:**

* Add execute permission: ``chmod +x script.sh``
* Check file ownership
* Verify platform permissions

**4. Missing Dependencies**

.. code-block:: text

   Error: Command not found: CMake

**Solution:**

* Install required dependencies
* Check PATH environment variable
* Use platform-specific package manager

API Reference
-------------

ValidationBuilder
~~~~~~~~~~~~~~~~~

Builder for creating validation workflows:

.. code-block:: python

   from script_validation import ValidationBuilder, Platform

   workflow = (ValidationBuilder()
       .root_path(Path('./'))
       .platforms(Platform.WINDOWS, Platform.LINUX)
       .validators('functional', 'compatibility')
       .report_formats('html', 'junit')
       .timeout(600)
       .max_memory(2048)
       .ci_mode(True)
       .parallel(True)
       .verbose(True)
       .build())

Platform Enum
~~~~~~~~~~~~~

Available platforms:

.. code-block:: python

   from script_validation import Platform

   Platform.WINDOWS  # Native Windows
   Platform.WSL      # Windows Subsystem for Linux
   Platform.LINUX    # Native Linux

ValidationResult
~~~~~~~~~~~~~~~~

Result of validation:

.. code-block:: python

   class ValidationResult:
       success: bool
       total_scripts: int
       passed: int
       failed: int
       platforms: List[Platform]
       failures: List[FailureInfo]
       duration: float

See Also
--------

* :doc:`validation_framework` - System validation framework
* :doc:`scripts` - Build scripts documentation
* :doc:`testing` - General testing guide
* :doc:`contributing` - Contribution guidelines
