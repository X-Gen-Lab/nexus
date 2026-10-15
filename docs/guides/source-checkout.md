---
id: docs-guides-source-checkout
type: how-to
audience: consumer
source_language: en
---

(docs-guides-source-checkout)=
# Obtain the exact source and pinned dependencies

Use this guide before running repository commands in a tutorial, configuration
reference or maintenance workflow. You will obtain a detached, exact Nexus source
revision and its recursive dependencies. This requires Git, Python 3.11+ and
network access for the initial clone and dependency retrieval. It does not build,
flash or establish software or hardware qualification.

## Select the revision described by your documentation

Open the immutable documentation version identified by the page's version
banner. Download `documentation-manifest.json` from that version's root, or use
the file already present at the root of its verified offline package. Keep it
outside your future checkout, for example as `nexus-documentation-manifest.json`
in your working parent directory. If you entered through `stable/`, record the
resolved immutable version before downloading; do not mix two versions.

| Manifest field | Use |
| --- | --- |
| `documentation_source.commit` | Exact source containing these pages and their tools |
| `code_source.commit` | Source described by the package; inspect its identity separately |
| `qualified_code.commit` | Referenced software qualification baseline, with its original scope and results |

The default GitHub branch may lag a reviewed documentation branch. A branch
name, successful clone or documentation release is not a substitute for the
exact commit. The previously reviewed documentation snapshot is
`5a7a75c321ea173df8692714f95a8df13715293d`; choosing it recreates that snapshot,
not later documentation. Its [immutable package manifest](https://x-gen-lab.github.io/nexus/versions/docs-20261014-5a7a75c321ea/documentation-manifest.json)
is one concrete example. Use your version's manifest for the current tutorial.

## Clone and initialize that exact revision

Run these POSIX-shell commands from the parent directory containing the downloaded
manifest. Choose a new checkout directory; the commands do not replace one that
already exists. The Python command validates the identity before passing it to
Git:

```sh
nexus_source_revision=$(python -c 'import json, re; from pathlib import Path; manifest = json.loads(Path("nexus-documentation-manifest.json").read_text()); revision = manifest["documentation_source"]["commit"]; assert re.fullmatch(r"[0-9a-f]{40}", revision), "invalid documentation commit"; print(revision)')
git clone --no-checkout https://github.com/X-Gen-Lab/nexus.git nexus
cd nexus
git checkout --detach "$nexus_source_revision"
git submodule sync --recursive
git submodule update --init --recursive
git rev-parse HEAD
git submodule status --recursive
git status --short
```

Check every command's exit status. `git rev-parse HEAD` must equal the manifest's
full commit. Recursive submodule lines must have neither a leading `-` (missing),
`+` (different revision) nor `U` (conflict), and `git status --short` must be empty
before starting a clean-source workflow. Stop on an unavailable commit or failed
dependency retrieval; never substitute `main`, `HEAD` or a latest dependency.
For a preview without a package manifest, record the full commit from its source
banner or `documentation-build.json` and use that validated commit explicitly.

On Windows, run the POSIX snippet in Git Bash, or read the same manifest field
and perform the corresponding Git commands in your shell. Do not treat the shell
variable syntax as a portable PowerShell command.

## Prepare only the tools required by your task

| Task | Next preparation |
| --- | --- |
| Native consumer or I/O fixtures | Host Python, CMake, Ninja and C/C++ compiler required by the selected tutorial |
| Configuration inspection | Python and the maintained resolver; ARM compilation remains a separate step |
| Platform maintenance | [Development tools, TDD and hooks](../development/workflow.md) |
| Documentation check, build or package | [Locked documentation tuple and unified CLI](../reference/documentation-tools.md) |
| MCU linking or hardware preparation | [Exact toolchain and HIL preparation](performance-hil.md) |

Each tutorial starts from the repository root established above. Keep independent
consumer and package outputs outside the source checkout when instructed. Retain
manifests, source identities and required reports before deleting only directories
you created. A clean checkout supplies input identity; actual execution and
[qualification evidence](../status/software-verification.md) remain separate.
