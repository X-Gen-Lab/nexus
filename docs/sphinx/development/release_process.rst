Release Process
===============

Guide to the Nexus release management process.

.. contents:: Table of Contents
   :local:
   :depth: 3

Overview
--------

The Nexus release process ensures:

* **Quality**: All releases are thoroughly tested
* **Stability**: Breaking changes are managed carefully
* **Documentation**: Changes are well documented
* **Traceability**: All changes are tracked

Release Types
-------------

Semantic Versioning
~~~~~~~~~~~~~~~~~~~

Nexus follows semantic versioning (MAJOR.MINOR.PATCH):

* **MAJOR**: Breaking changes
* **MINOR**: New features (backward compatible)
* **PATCH**: Bug fixes (backward compatible)

**Examples**:

* ``1.0.0`` → ``1.0.1``: Bug fix release
* ``1.0.0`` → ``1.1.0``: Feature release
* ``1.0.0`` → ``2.0.0``: Major release with breaking changes

Release Schedule
~~~~~~~~~~~~~~~~

**Regular Releases**

* **Patch releases**: As needed for critical bugs
* **Minor releases**: Every 2-3 months
* **Major releases**: Once or twice per year

**Release Candidates**

* RC1, RC2, etc. before major/minor releases
* At least 1 week of testing before final release

Release Process
---------------

Step 1: Planning
~~~~~~~~~~~~~~~~

**Create Release Branch**

.. code-block:: bash

   # Create release branch from develop
   git checkout develop
   git pull origin develop
   git checkout -b release/v1.2.0

**Update Version Numbers**

Update version in:

* ``CMakeLists.txt``
* ``version.h``
* ``package.json`` (if applicable)
* Documentation

.. code-block:: cmake

   # CMakeLists.txt
   project(Nexus
       VERSION 1.2.0
       LANGUAGES C CXX
   )

Step 2: Testing
~~~~~~~~~~~~~~~

**Run Full Test Suite**

.. code-block:: bash

   # Run all tests
   python scripts/test/test.py --all

   # Run on all platforms
   python scripts/test/test.py --platform native
   python scripts/test/test.py --platform stm32f4

   # Check coverage
   python scripts/test/test.py --coverage

**Hardware Testing**

* Test on all supported hardware platforms
* Verify all examples work
* Test upgrade from previous version

**Performance Testing**

.. code-block:: bash

   # Run performance benchmarks
   python scripts/test/benchmark.py

   # Compare with previous release
   python scripts/test/benchmark.py --compare v1.1.0

Step 3: Documentation
~~~~~~~~~~~~~~~~~~~~~~

**Update CHANGELOG**

.. code-block:: markdown

   # Changelog

   ## [1.2.0] - 2026-01-25

   ### Added
   - New SPI DMA support
   - Configuration system improvements
   - Additional examples

   ### Changed
   - Improved error handling in HAL
   - Updated documentation

   ### Fixed
   - GPIO initialization bug
   - UART timeout handling

   ### Deprecated
   - Old configuration API (use new API)

**Update Documentation**

* API documentation (Doxygen)
* User guides
* Migration guides (for breaking changes)
* Release notes

**Generate Documentation**

.. code-block:: bash

   # Generate API docs
   python scripts/docs/generate.py

   # Build Sphinx docs
   cd docs/sphinx
   make html

Step 4: Code Review
~~~~~~~~~~~~~~~~~~~

**Create Pull Request**

.. code-block:: bash

   # Push release branch
   git push origin release/v1.2.0

   # Create PR: release/v1.2.0 → main

**Review Checklist**

☐ All tests pass
☐ Documentation updated
☐ CHANGELOG updated
☐ Version numbers updated
☐ No debug code
☐ Code formatted
☐ Static analysis clean

Step 5: Release Candidate
~~~~~~~~~~~~~~~~~~~~~~~~~~

**Tag Release Candidate**

.. code-block:: bash

   # Tag RC
   git tag -a v1.2.0-rc1 -m "Release candidate 1 for v1.2.0"
   git push origin v1.2.0-rc1

**Announce RC**

* Post in GitHub Discussions
* Notify testers
* Request feedback

**Testing Period**

* Minimum 1 week for RC testing
* Fix critical bugs found
* Create new RC if needed

Step 6: Final Release
~~~~~~~~~~~~~~~~~~~~~~

**Merge to Main**

.. code-block:: bash

   # Merge release branch to main
   git checkout main
   git merge --no-ff release/v1.2.0
   git push origin main

**Tag Release**

.. code-block:: bash

   # Create release tag
   git tag -a v1.2.0 -m "Release v1.2.0"
   git push origin v1.2.0

**Merge Back to Develop**

.. code-block:: bash

   # Merge to develop
   git checkout develop
   git merge --no-ff release/v1.2.0
   git push origin develop

   # Delete release branch
   git branch -d release/v1.2.0
   git push origin --delete release/v1.2.0

Step 7: Publish Release
~~~~~~~~~~~~~~~~~~~~~~~~

**Create GitHub Release**

1. Go to GitHub Releases
2. Click "Draft a new release"
3. Select tag ``v1.2.0``
4. Add release notes
5. Attach release artifacts
6. Publish release

**Release Artifacts**

* Source code (automatic)
* Binary releases (if applicable)
* Documentation (PDF/HTML)
* Examples

**Announce Release**

* GitHub Discussions
* Project website
* Social media
* Mailing list

Hotfix Process
--------------

Critical Bug Fixes
~~~~~~~~~~~~~~~~~~

**Create Hotfix Branch**

.. code-block:: bash

   # Create hotfix from main
   git checkout main
   git checkout -b hotfix/v1.2.1

**Fix Bug**

.. code-block:: bash

   # Make fix
   # Add tests
   # Update CHANGELOG

**Release Hotfix**

.. code-block:: bash

   # Merge to main
   git checkout main
   git merge --no-ff hotfix/v1.2.1
   git tag -a v1.2.1 -m "Hotfix v1.2.1"
   git push origin main v1.2.1

   # Merge to develop
   git checkout develop
   git merge --no-ff hotfix/v1.2.1
   git push origin develop

   # Delete hotfix branch
   git branch -d hotfix/v1.2.1

Release Checklist
-----------------

Pre-Release
~~~~~~~~~~~

☐ All tests pass
☐ Code coverage meets target (>90%)
☐ Documentation updated
☐ CHANGELOG updated
☐ Version numbers updated
☐ Migration guide (if breaking changes)
☐ Release notes prepared
☐ Hardware testing complete
☐ Performance benchmarks run

Release
~~~~~~~

☐ Release branch created
☐ Release candidate tested
☐ Final review complete
☐ Merged to main
☐ Tagged
☐ GitHub release created
☐ Artifacts uploaded
☐ Announcement posted

Post-Release
~~~~~~~~~~~~

☐ Merged back to develop
☐ Release branch deleted
☐ Documentation published
☐ Monitor for issues
☐ Respond to feedback

Version Support
---------------

Support Policy
~~~~~~~~~~~~~~

* **Current version**: Full support
* **Previous minor version**: Security fixes only
* **Older versions**: No support

**Example**:

* Current: v1.2.0 (full support)
* Previous: v1.1.x (security fixes)
* Older: v1.0.x (no support)

Long-Term Support (LTS)
~~~~~~~~~~~~~~~~~~~~~~~

* LTS releases supported for 2 years
* Security and critical bug fixes only
* Marked as LTS in release notes

See Also
--------

* :doc:`contributing` - Contribution guidelines
* :doc:`testing` - Testing guidelines
* :doc:`code_review_guidelines` - Code review process

Summary
-------

The release process ensures quality through:

1. **Planning**: Version numbers, release branch
2. **Testing**: Full test suite, hardware testing
3. **Documentation**: CHANGELOG, docs, release notes
4. **Review**: Code review, final checks
5. **RC**: Release candidate testing
6. **Release**: Tag, merge, publish
7. **Announce**: GitHub, website, social media

Following this process ensures stable, well-documented releases.
