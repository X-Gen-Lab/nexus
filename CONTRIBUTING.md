# Contributing to Nexus

Nexus maintains reusable embedded mechanisms. Product code belongs in an
external repository. Read [AGENTS.md](AGENTS.md), [the design contracts](docs/design/README.md)
and [current delivery](docs/delivery/README.md) before changing a boundary.

```sh
git clone --recurse-submodules https://github.com/X-Gen-Lab/nexus.git
cd nexus
python scripts/setup/install_dev_tools.py
.venv/bin/python -m pip install -r requirements.txt
python tools/dev/dev.py configure --preset native-debug
python tools/dev/dev.py build --preset native-debug --parallel 4
python tools/dev/dev.py test --preset native-debug
```

Install the pre-commit and commit-msg hooks for every clone. They check staged
contents, preserve existing hook migration, use pinned clang-format 14.0.6 and
never auto-stage edits. CI runs the same whole-file format/comment/text checks.
Conventional Commits use `type(scope): description`; do not bypass hooks.

Keep the existing root `.clang-format`, `.editorconfig`, 80-column layout and
backslash Doxygen conventions. Headers document public ownership, context,
deadlines and failure contracts; source comments explain invariants and hardware
ordering. Vendor code stays outside owned formatting. Historical style debt may
only shrink. Changed files receive strict whole-file checks.

```sh
.venv/bin/python -m pre_commit run nexus-style --all-files --show-diff-on-failure
.venv/bin/python -m unittest discover -s tools/configure -p 'test_*.py'
doxygen Doxyfile
.venv/bin/python -m sphinx -W --keep-going -b html docs/sphinx build/docs
```

Choose validation for the behavior changed: lifecycle/IRQ/concurrency require
fault and late-event regressions; storage requires interrupted writes; config
requires rejection and resource-conflict cases. Compile reusable components for
both MCU families. Native models, ARM linking and physical station execution
have different scopes; record hardware tests as unexecuted until equipment runs.

PR descriptions explain the resulting behavior, actual validation and limits.
Platform changes include exact part/board/backend/mode and raw evidence paths.
CI enforces style, analyzers, host contracts, six ARM assemblies, tool boundaries
and docs; review remains a maintainer responsibility. Repository protection and
physical qualification are not implied by a workflow definition.
