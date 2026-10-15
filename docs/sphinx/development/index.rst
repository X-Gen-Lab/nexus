Maintaining the platform
========================

.. toctree::
   :maxdepth: 1

   coding_standards

Use ``tools/dev/dev.py`` for configure/build/test and repository checks. Install
Git hooks with ``scripts/setup/install_dev_tools.py``. Staged files and CI share
whole-file format/comment rules; commit messages follow Conventional Commits.
Third-party SDK source stays unchanged and verified against its lock.

Record each concrete trigger, resulting behavior, relevant failure-path test
and exact unsupported or unexecuted scope. High-risk changes need real
production-source model execution and applicable ARM linkage. Implementation
paths and exits are tracked in ``docs/design/next-generation-execution.csv``;
current evidence and remaining physical work are in ``docs/delivery/``.
