---
id: docs-guides-source-checkout
type: how-to
audience: consumer
source_language: en
---

(docs-guides-source-checkout)=
# Obtain the exact source and pinned dependencies

Use this guide before executing a repository tutorial. You will obtain an exact
Nexus commit and the dependencies selected by your task. Initial retrieval needs
Git, Python 3.11+ and network access; it does not build or flash firmware.

## Select the documented source commit

Read the full commit from the documentation source link, source banner or
`source.commit` in `documentation-build.json`. A dirty documentation preview is
not an immutable source delivery. Record an approved clean commit before using
these commands; branch names and version strings do not identify exact bytes.

Historical documentation packages retain their original manifests and fields.
Use their matching fixed instructions when recovering them. Current builds write
`documentation-build.json`; a documentation archive manifest is not required to
obtain current source. Historical reports do not establish current qualification.

## Retrieve only that source revision

Run from a working parent directory. Replace the placeholder with the selected
40-character commit; choose a new checkout directory:

```sh
nexus_source_revision=REPLACE_WITH_APPROVED_40_CHARACTER_COMMIT
python -c 'import re, sys; assert re.fullmatch(r"[0-9a-f]{40}", sys.argv[1]), "invalid source commit"' "$nexus_source_revision"
git init nexus
cd nexus
git remote add origin https://github.com/X-Gen-Lab/nexus.git
git fetch --no-tags origin "$nexus_source_revision"
git checkout --detach "$nexus_source_revision"
python tools/dev/dev.py prepare --scope native --allow-network
git rev-parse HEAD
git status --short
```

Check each exit status and stop on failure. HEAD must match the selected commit
and the checkout must be clean. Preparation checks the catalogue, parent gitlinks
and only the selected recursive dependency closure. Unselected inventory may
remain uninitialized; `git submodule status --recursive` therefore does not
require every repository dependency to be populated. Firmware configuration
never downloads dependencies or substitutes a newer revision.

For an MCU task, prepare the exact maintained preset instead of unrelated SDKs:

```sh
python tools/dev/dev.py prepare \
    --preset stm32-qiming-baremetal --allow-network
```

A verified Source SDK is an independent consumption path. Its full prefix contains
source, selected dependencies, licenses and CMake package metadata; it does not
need the repository's historical artifact branches. Follow the
[Source SDK tutorial](../tutorials/source-sdk.md) for export and consumption.

## Prepare tools for the selected task

| Task | Required preparation |
| --- | --- |
| Native consumer or models | Python, CMake, Ninja and the maintained host compiler |
| Configuration inspection | Python and the current resolver |
| Platform maintenance | [Development tools, TDD and hooks](../development/workflow.md) |
| Documentation | [Locked documentation tools and CLI](../reference/documentation-tools.md) |
| MCU linking or HIL preparation | [Exact toolchain and station preparation](performance-hil.md) |

Tutorials run from the repository root. Keep independent consumers outside that
checkout and retain actual reports before removing only your own output. A clean
source identity does not replace tests, independent SDK checks or physical HIL.
