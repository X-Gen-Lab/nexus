CI/CD Integration
=================

This guide covers integrating the Nexus Embedded Platform with various CI/CD systems including GitHub Actions, GitLab CI, Jenkins, and Azure DevOps.

.. contents:: Table of Contents
   :local:
   :depth: 2

Overview
--------

The Nexus platform provides comprehensive CI/CD integration for:

* **Automated Building**: Cross-platform builds for all supported targets
* **Testing**: Unit tests, property-based tests, and integration tests
* **Coverage Analysis**: Code coverage tracking and reporting
* **Documentation**: Automated documentation generation and deployment
* **Script Validation**: Cross-platform script verification
* **Artifact Management**: Build artifacts and reports

GitHub Actions
--------------

GitHub Actions is the recommended CI/CD platform for the Nexus project.

Basic Workflow
~~~~~~~~~~~~~~

Create ``.github/workflows/ci.yml``:

.. code-block:: yaml

   name: CI

   on:
     push:
       branches: [ main, develop ]
     pull_request:
       branches: [ main, develop ]

   jobs:
     build-and-test:
       runs-on: ${{ matrix.os }}
       strategy:
         matrix:
           os: [ubuntu-latest, windows-latest, macos-latest]
           build_type: [Debug, Release]

       steps:
         - uses: actions/checkout@v3
           with:
             submodules: recursive

         - name: Set up Python
           uses: actions/setup-python@v4
           with:
             python-version: '3.10'

         - name: Install dependencies (Ubuntu)
           if: runner.os == 'Linux'
           run: |
             sudo apt-get update
             sudo apt-get install -y CMake ninja-build lcov

         - name: Install dependencies (macOS)
           if: runner.os == 'macOS'
           run: |
             brew install CMake ninja lcov

         - name: Install dependencies (Windows)
           if: runner.os == 'Windows'
           run: |
             choco install CMake ninja

         - name: Configure CMake
           run: |
             CMake -B build -G Ninja \
               -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} \
               -DNEXUS_BUILD_TESTS=ON

         - name: Build
           run: CMake --build build --config ${{ matrix.build_type }}

         - name: Test
           run: ctest --test-dir build --output-on-failure

         - name: Upload artifacts
           if: failure()
           uses: actions/upload-artifact@v3
           with:
             name: build-artifacts-${{ matrix.os }}-${{ matrix.build_type }}
             path: build/

Coverage Workflow
~~~~~~~~~~~~~~~~~

Create ``.github/workflows/coverage.yml``:

.. code-block:: yaml

   name: Coverage

   on:
     push:
       branches: [ main ]
     pull_request:
       branches: [ main ]

   jobs:
     coverage:
       runs-on: ubuntu-latest

       steps:
         - uses: actions/checkout@v3
           with:
             submodules: recursive

         - name: Install dependencies
           run: |
             sudo apt-get update
             sudo apt-get install -y CMake ninja-build lcov python3-pip
             pip install -r scripts/validation/requirements.txt

         - name: Run validation with coverage
           run: |
             python scripts/validation/validate.py \
               --coverage \
               --threshold 0.80 \
               --report-dir validation_reports

         - name: Upload coverage to Codecov
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

Documentation Workflow
~~~~~~~~~~~~~~~~~~~~~~

Create ``.github/workflows/docs.yml``:

.. code-block:: yaml

   name: Documentation

   on:
     push:
       branches: [ main ]
     pull_request:
       branches: [ main ]

   jobs:
     build-docs:
       runs-on: ubuntu-latest

       steps:
         - uses: actions/checkout@v3

         - name: Set up Python
           uses: actions/setup-python@v4
           with:
             python-version: '3.10'

         - name: Install dependencies
           run: |
             sudo apt-get update
             sudo apt-get install -y doxygen graphviz
             pip install -r docs/sphinx/requirements.txt

         - name: Build Doxygen
           run: doxygen Doxyfile

         - name: Build Sphinx (English)
           run: |
             cd docs/sphinx
             sphinx-build -b html . _build/html/en

         - name: Build Sphinx (Chinese)
           run: |
             cd docs/sphinx
             sphinx-build -b html -D language=zh_CN . _build/html/zh_CN

         - name: Deploy to GitHub Pages
           if: github.ref == 'refs/heads/main'
           uses: peaceiris/actions-gh-pages@v3
           with:
             github_token: ${{ secrets.GITHUB_TOKEN }}
             publish_dir: ./docs/sphinx/_build/html

Script Validation Workflow
~~~~~~~~~~~~~~~~~~~~~~~~~~

Create ``.github/workflows/scripts.yml``:

.. code-block:: yaml

   name: Script Validation

   on:
     push:
       branches: [ main, develop ]
     pull_request:
       branches: [ main, develop ]

   jobs:
     validate-scripts:
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

         - name: Validate scripts
           run: python -m script_validation --ci --report-format junit

         - name: Publish test results
           if: always()
           uses: EnricoMi/publish-unit-test-result-action@v2
           with:
             files: validation_reports/junit.xml

         - name: Upload report
           if: always()
           uses: actions/upload-artifact@v3
           with:
             name: script-validation-${{ matrix.os }}
             path: validation_reports/

Complete CI Pipeline
~~~~~~~~~~~~~~~~~~~~

Create ``.github/workflows/pipeline.yml``:

.. code-block:: yaml

   name: Complete Pipeline

   on:
     push:
       branches: [ main ]
     pull_request:
       branches: [ main ]

   jobs:
     build:
       runs-on: ${{ matrix.os }}
       strategy:
         matrix:
           os: [ubuntu-latest, windows-latest]
           build_type: [Debug, Release]

       steps:
         - uses: actions/checkout@v3
           with:
             submodules: recursive

         - name: Build and Test
           run: |
             CMake -B build -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} -DNEXUS_BUILD_TESTS=ON
             CMake --build build
             ctest --test-dir build --output-on-failure

     coverage:
       needs: build
       runs-on: ubuntu-latest
       steps:
         - uses: actions/checkout@v3
           with:
             submodules: recursive
         - name: Coverage Analysis
           run: python scripts/validation/validate.py --coverage
         - uses: codecov/codecov-action@v3
           with:
             files: ./build/coverage.info

     docs:
       needs: build
       runs-on: ubuntu-latest
       steps:
         - uses: actions/checkout@v3
         - name: Build Documentation
           run: |
             pip install -r docs/sphinx/requirements.txt
             cd docs/sphinx && sphinx-build -b html . _build/html
         - uses: peaceiris/actions-gh-pages@v3
           if: github.ref == 'refs/heads/main'
           with:
             github_token: ${{ secrets.GITHUB_TOKEN }}
             publish_dir: ./docs/sphinx/_build/html

     scripts:
       needs: build
       runs-on: ubuntu-latest
       steps:
         - uses: actions/checkout@v3
         - name: Validate Scripts
           run: python -m script_validation --ci

GitLab CI
---------

GitLab CI provides powerful pipeline features with excellent Docker integration.

Basic Pipeline
~~~~~~~~~~~~~~

Create ``.gitlab-ci.yml``:

.. code-block:: yaml

   stages:
     - build
     - test
     - coverage
     - docs
     - deploy

   variables:
     GIT_SUBMODULE_STRATEGY: recursive

   .build_template: &build_template
     stage: build
     script:
       - CMake -B build -G Ninja -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DNEXUS_BUILD_TESTS=ON
       - CMake --build build
     artifacts:
       paths:
         - build/
       expire_in: 1 hour

   build:linux:debug:
     <<: *build_template
     image: ubuntu:22.04
     variables:
       BUILD_TYPE: Debug
     before_script:
       - apt-get update && apt-get install -y CMake ninja-build gcc g++ python3 python3-pip

   build:linux:release:
     <<: *build_template
     image: ubuntu:22.04
     variables:
       BUILD_TYPE: Release
     before_script:
       - apt-get update && apt-get install -y CMake ninja-build gcc g++ python3 python3-pip

   test:
     stage: test
     image: ubuntu:22.04
     dependencies:
       - build:linux:debug
     script:
       - cd build && ctest --output-on-failure
     artifacts:
       reports:
         junit: build/test_results.xml

Coverage Pipeline
~~~~~~~~~~~~~~~~~

.. code-block:: yaml

   coverage:
     stage: coverage
     image: ubuntu:22.04
     dependencies:
       - build:linux:debug
     before_script:
       - apt-get update && apt-get install -y lcov python3-pip
       - pip3 install -r scripts/validation/requirements.txt
     script:
       - python3 scripts/validation/validate.py --coverage --threshold 0.80
     coverage: '/line_coverage: (\d+\.\d+)%/'
     artifacts:
       reports:
         coverage_report:
           coverage_format: cobertura
           path: validation_reports/coverage.xml
       paths:
         - validation_reports/
       expire_in: 30 days

Documentation Pipeline
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: yaml

   docs:
     stage: docs
     image: python:3.10
     before_script:
       - apt-get update && apt-get install -y doxygen graphviz
       - pip install -r docs/sphinx/requirements.txt
     script:
       - doxygen Doxyfile
       - cd docs/sphinx && sphinx-build -b html . _build/html
     artifacts:
       paths:
         - docs/sphinx/_build/html
       expire_in: 30 days

   pages:
     stage: deploy
     dependencies:
       - docs
     script:
       - mkdir -p public
       - cp -r docs/sphinx/_build/html/* public/
     artifacts:
       paths:
         - public
     only:
       - main

Multi-Platform Pipeline
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: yaml

   .test_template: &test_template
     stage: test
     script:
       - python3 scripts/validation/validate.py --ci

   test:linux:
     <<: *test_template
     image: ubuntu:22.04
     tags:
       - linux

   test:windows:
     <<: *test_template
     tags:
       - windows
     before_script:
       - choco install python CMake ninja

   test:macos:
     <<: *test_template
     tags:
       - macos
     before_script:
       - brew install python CMake ninja

Jenkins
-------

Jenkins provides flexible pipeline configuration with extensive plugin support.

Declarative Pipeline
~~~~~~~~~~~~~~~~~~~~

Create ``Jenkinsfile``:

.. code-block:: groovy

   pipeline {
       agent any

       parameters {
           choice(name: 'BUILD_TYPE', choices: ['Debug', 'Release'], description: 'Build type')
           booleanParam(name: 'RUN_COVERAGE', defaultValue: true, description: 'Run coverage analysis')
           booleanParam(name: 'BUILD_DOCS', defaultValue: true, description: 'Build documentation')
       }

       environment {
           NEXUS_BUILD_DIR = 'build'
           NEXUS_COVERAGE_THRESHOLD = '0.80'
       }

       stages {
           stage('Checkout') {
               steps {
                   checkout scm
                   sh 'git submodule update --init --recursive'
               }
           }

           stage('Build') {
               parallel {
                   stage('Linux') {
                       agent { label 'linux' }
                       steps {
                           sh """
                               CMake -B ${NEXUS_BUILD_DIR} -G Ninja \
                                   -DCMAKE_BUILD_TYPE=${params.BUILD_TYPE} \
                                   -DNEXUS_BUILD_TESTS=ON
                               CMake --build ${NEXUS_BUILD_DIR}
                           """
                       }
                   }
                   stage('Windows') {
                       agent { label 'windows' }
                       steps {
                           bat """
                               CMake -B ${NEXUS_BUILD_DIR} -G Ninja ^
                                   -DCMAKE_BUILD_TYPE=${params.BUILD_TYPE} ^
                                   -DNEXUS_BUILD_TESTS=ON
                               CMake --build ${NEXUS_BUILD_DIR}
                           """
                       }
                   }
               }
           }

           stage('Test') {
               steps {
                   sh "cd ${NEXUS_BUILD_DIR} && ctest --output-on-failure"
               }
               post {
                   always {
                       junit "${NEXUS_BUILD_DIR}/test_results.xml"
                   }
               }
           }

           stage('Coverage') {
               when {
                   expression { params.RUN_COVERAGE }
               }
               steps {
                   sh """
                       python scripts/validation/validate.py \
                           --coverage \
                           --threshold ${NEXUS_COVERAGE_THRESHOLD}
                   """
               }
               post {
                   always {
                       publishHTML([
                           reportDir: 'validation_reports/coverage_html',
                           reportFiles: 'index.html',
                           reportName: 'Coverage Report'
                       ])
                   }
               }
           }

           stage('Documentation') {
               when {
                   expression { params.BUILD_DOCS }
               }
               steps {
                   sh """
                       pip install -r docs/sphinx/requirements.txt
                       doxygen Doxyfile
                       cd docs/sphinx && sphinx-build -b html . _build/html
                   """
               }
               post {
                   always {
                       publishHTML([
                           reportDir: 'docs/sphinx/_build/html',
                           reportFiles: 'index.html',
                           reportName: 'Documentation'
                       ])
                   }
               }
           }

           stage('Script Validation') {
               steps {
                   sh 'python -m script_validation --ci --report-format junit'
               }
               post {
                   always {
                       junit 'validation_reports/junit.xml'
                   }
               }
           }
       }

       post {
           always {
               archiveArtifacts artifacts: 'build/**/*', allowEmptyArchive: true
               archiveArtifacts artifacts: 'validation_reports/**/*', allowEmptyArchive: true
           }
           success {
               echo 'Pipeline completed successfully!'
           }
           failure {
               echo 'Pipeline failed!'
               emailext (
                   subject: "Build Failed: ${env.JOB_NAME} - ${env.BUILD_NUMBER}",
                   body: "Check console output at ${env.BUILD_URL}",
                   to: "${env.CHANGE_AUTHOR_EMAIL}"
               )
           }
       }
   }

Scripted Pipeline
~~~~~~~~~~~~~~~~~

.. code-block:: groovy

   node {
       stage('Checkout') {
           checkout scm
           sh 'git submodule update --init --recursive'
       }

       stage('Build') {
           try {
               sh """
                   CMake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DNEXUS_BUILD_TESTS=ON
                   CMake --build build
               """
           } catch (Exception e) {
               currentBuild.result = 'FAILURE'
               throw e
           }
       }

       stage('Test') {
           try {
               sh 'cd build && ctest --output-on-failure'
           } catch (Exception e) {
               currentBuild.result = 'UNSTABLE'
           } finally {
               junit 'build/test_results.xml'
           }
       }

       stage('Coverage') {
           sh 'python scripts/validation/validate.py --coverage'
           publishHTML([
               reportDir: 'validation_reports/coverage_html',
               reportFiles: 'index.html',
               reportName: 'Coverage Report'
           ])
       }
   }

Azure DevOps
------------

Azure DevOps provides integrated CI/CD with Azure cloud services.

Basic Pipeline
~~~~~~~~~~~~~~

Create ``azure-pipelines.yml``:

.. code-block:: yaml

   trigger:
     branches:
       include:
         - main
         - develop

   pr:
     branches:
       include:
         - main
         - develop

   pool:
     vmImage: 'ubuntu-latest'

   variables:
     buildConfiguration: 'Release'

   steps:
     - checkout: self
       submodules: true

     - task: UsePythonVersion@0
       inputs:
         versionSpec: '3.10'
       displayName: 'Use Python 3.10'

     - script: |
         sudo apt-get update
         sudo apt-get install -y CMake ninja-build lcov
       displayName: 'Install dependencies'

     - script: |
         CMake -B build -G Ninja \
           -DCMAKE_BUILD_TYPE=$(buildConfiguration) \
           -DNEXUS_BUILD_TESTS=ON
         CMake --build build
       displayName: 'Build'

     - script: |
         cd build && ctest --output-on-failure
       displayName: 'Test'

     - task: PublishTestResults@2
       condition: always()
       inputs:
         testResultsFormat: 'JUnit'
         testResultsFiles: 'build/test_results.xml'
       displayName: 'Publish test results'

Multi-Platform Pipeline
~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: yaml

   strategy:
     matrix:
       Linux:
         imageName: 'ubuntu-latest'
       Windows:
         imageName: 'windows-latest'
       macOS:
         imageName: 'macos-latest'

   pool:
     vmImage: $(imageName)

   steps:
     - checkout: self
       submodules: true

     - task: UsePythonVersion@0
       inputs:
         versionSpec: '3.10'

     - script: |
         CMake -B build -DCMAKE_BUILD_TYPE=Release -DNEXUS_BUILD_TESTS=ON
         CMake --build build
       displayName: 'Build'

     - script: |
         cd build && ctest --output-on-failure
       displayName: 'Test'

Coverage Pipeline
~~~~~~~~~~~~~~~~~

.. code-block:: yaml

   - script: |
       pip install -r scripts/validation/requirements.txt
       python scripts/validation/validate.py --coverage --threshold 0.80
     displayName: 'Run coverage analysis'

   - task: PublishCodeCoverageResults@1
     inputs:
       codeCoverageTool: 'Cobertura'
       summaryFileLocation: 'validation_reports/coverage.xml'
       reportDirectory: 'validation_reports/coverage_html'
     displayName: 'Publish coverage results'

Documentation Pipeline
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: yaml

   - script: |
       sudo apt-get install -y doxygen graphviz
       pip install -r docs/sphinx/requirements.txt
     displayName: 'Install documentation tools'

   - script: |
       doxygen Doxyfile
       cd docs/sphinx && sphinx-build -b html . _build/html
     displayName: 'Build documentation'

   - task: PublishBuildArtifacts@1
       inputs:
       pathToPublish: 'docs/sphinx/_build/html'
       artifactName: 'documentation'
     displayName: 'Publish documentation'

Best Practices
--------------

Caching
~~~~~~~

**GitHub Actions:**

.. code-block:: yaml

   - name: Cache CMake build
     uses: actions/cache@v3
     with:
       path: build
       key: ${{ runner.os }}-build-${{ hashFiles('**/CMakeLists.txt') }}

**GitLab CI:**

.. code-block:: yaml

   cache:
     key: ${CI_COMMIT_REF_SLUG}
     paths:
       - build/
       - .cache/pip

**Jenkins:**

.. code-block:: groovy

   options {
       buildDiscarder(logRotator(numToKeepStr: '10'))
       disableConcurrentBuilds()
   }

Parallel Execution
~~~~~~~~~~~~~~~~~~

**GitHub Actions:**

.. code-block:: yaml

   strategy:
     matrix:
       os: [ubuntu-latest, windows-latest, macos-latest]
       build_type: [Debug, Release]
     fail-fast: false

**GitLab CI:**

.. code-block:: yaml

   test:
     parallel:
       matrix:
         - PLATFORM: [linux, windows, macos]
           BUILD_TYPE: [Debug, Release]

**Jenkins:**

.. code-block:: groovy

   parallel {
       stage('Linux') { ... }
       stage('Windows') { ... }
       stage('macOS') { ... }
   }

Artifact Management
~~~~~~~~~~~~~~~~~~~

**GitHub Actions:**

.. code-block:: yaml

   - uses: actions/upload-artifact@v3
     with:
       name: build-artifacts
       path: |
         build/
         validation_reports/
       retention-days: 30

**GitLab CI:**

.. code-block:: yaml

   artifacts:
     paths:
       - build/
       - validation_reports/
     expire_in: 30 days

**Jenkins:**

.. code-block:: groovy

   archiveArtifacts artifacts: 'build/**/*', allowEmptyArchive: true

Notifications
~~~~~~~~~~~~~

**GitHub Actions:**

.. code-block:: yaml

   - name: Notify on failure
     if: failure()
     uses: 8398a7/action-slack@v3
     with:
       status: ${{ job.status }}
       webhook_url: ${{ secrets.SLACK_WEBHOOK }}

**GitLab CI:**

.. code-block:: yaml

   after_script:
     - 'curl -X POST -H "Content-Type: application/json" -d "{\"text\":\"Build $CI_JOB_STATUS\"}" $SLACK_WEBHOOK'

**Jenkins:**

.. code-block:: groovy

   post {
       failure {
           slackSend channel: '#builds', message: "Build failed: ${env.JOB_NAME}"
       }
   }

Troubleshooting
---------------

Common Issues
~~~~~~~~~~~~~

**1. Submodule Checkout Failure**

.. code-block:: yaml

   # GitHub Actions
   - uses: actions/checkout@v3
     with:
       submodules: recursive
       token: ${{ secrets.GITHUB_TOKEN }}

**2. Build Cache Corruption**

.. code-block:: bash

   # Clear cache and rebuild
   rm -rf build
   CMake -B build -G Ninja
   CMake --build build

**3. Test Timeout**

.. code-block:: yaml

   # Increase timeout
   - run: ctest --test-dir build --timeout 600

**4. Coverage Upload Failure**

.. code-block:: bash

   # Verify coverage file exists
   ls -la build/coverage.info

   # Check file format
   lcov --list build/coverage.info

See Also
--------

* :doc:`validation_framework` - System validation framework
* :doc:`script_validation` - Script validation system
* :doc:`scripts` - Build scripts documentation
* :doc:`testing` - General testing guide
