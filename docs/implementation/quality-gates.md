# Required quality gates

## Commit style checks

The repository installs real `pre-commit` and `commit-msg` hooks through
[install_dev_tools.py](../../scripts/setup/install_dev_tools.py). Python 3.10 or
newer is required. From the repository root:

```sh
python scripts/setup/install_dev_tools.py
source .venv/bin/activate
```

The activation command above is for Linux, macOS and WSL. Windows PowerShell uses
`.\.venv\Scripts\Activate.ps1`; Command Prompt uses `.venv\Scripts\activate.bat`.
Activate the environment in each development shell. The version authority is
[development-tools.txt](../../dependencies/development-tools.txt), currently
**pre-commit 4.3.0** and **clang-format 14.0.6**. The installer creates or reuses
the repository `.venv`, skips pip when installed versions match, and verifies
the actual distributions and runnable tool versions. Missing dependencies and
failed version detection return nonzero.

Every new clone needs hook installation because Git hooks are local metadata.
The installer uses pre-commit's default migration for existing user hooks; it
does not use `--overwrite` or change global Git configuration. An explicit
`core.hooksPath` is rejected with a diagnostic. `--skip-hooks` installs tools
without altering hooks and is the CI mode.

### Staged snapshot and manual checks

On `git commit`, pre-commit isolates the staged snapshot and restores unstaged
changes after the check. A clean working copy cannot conceal bad staged bytes,
and unrelated unstaged edits are not included in the commit check. The
`nexus-style` hook checks selected source/text paths; `nexus-commit-message`
checks the Conventional Commit header at the `commit-msg` stage.

Manual pre-commit commands inspect working-tree contents, even though their
path selection comes from the index:

```sh
python -m pre_commit run nexus-style --files hal/src/nx_device.c
python -m pre_commit run nexus-style --all-files --show-diff-on-failure
```

These commands do not certify a different partially staged snapshot. Failed
checks report errors and may write the diagnostic JSON report, but do not
format source or run `git add`. Contributors fix and review changes, then stage
the intended content explicitly. The hook report is
`build/quality/style-gate.json`; source paths, hashes, tool version, legacy debt
and failures are recorded there.

### Owned scope and frozen legacy debt

The root `.clang-format` is the formatter policy; nested styles and implicit
formatter fallback do not change it. `.clang-format-dirs` defines owned source
roots, extensions and exclusions, including the vendor/build boundaries.
Explicit file selection cannot override those exclusions. Unknown source
ownership, empty owned scope, invalid policy, missing tools and tool failures
fail the gate. Changes to style control files expand the check to the full
owned source set.

The initial audit covered **596 owned C/C++ files**, recording frozen format
debt for **337 files** and mechanical comment debt for **291 files**; the sets
overlap. [style-baseline.json](../../dependencies/style-baseline.json) binds
each allowance to exact SHA-256 source bytes and rule counts from the fixed
historical source `b68922d1b29291734a2b056f3b8229a0bebd654c`. These counts are an
initial inventory, not an adjustable debt budget or a whole-repository style
pass.

Only unchanged historical bytes can use an allowance. The source must match
both the frozen hash and the selected trusted base; new or modified files are
strict, including the rest of that file's existing debt. The first installation
checks a baseline against the fixed historical source when its trusted base
has no baseline. Later changes cannot add debt, increase rule counts, replace
hashes or rebase the sealed source/formatter identity. Remove resolved entries
or reduce debt; never raise the baseline to make failing code pass.

Mechanical comment checks cover comment syntax, backslash Doxygen tags,
section separators, file-header fields and placement of API documentation.
They do not prove API coverage or the meaning of ownership, ISR context,
deadlines, cancellation, callback lifetime or failure recovery. Those contracts
still need review and suitable behavior tests.

Strict formatter-only commands bypass the legacy allowance:

```sh
python scripts/tools/format.py --check --all
python scripts/tools/format.py --check --files hal/src/nx_device.c
python scripts/tools/format.py --help
```

`--all` selects all owned index paths and reads current working-tree bytes;
`--files` uses explicit owned paths. A strict full check can fail on frozen
legacy files that the style hook permits unchanged. Exit 0 means all selected
files pass, 1 means formatting fails, and 2 means invalid input or failed
preflight. Omitting `--check` deliberately formats selected files in place;
this separate operation never stages them.

Changed-text checks cover UTF-8, LF line endings, final newline, NUL rejection,
trailing whitespace outside Markdown, and Python syntax where applicable.
They do not replace compilation or static analysis.

### CI parity and separate build evidence

[quality-checks.yml](../../.github/workflows/quality-checks.yml) installs the
same lock using `python scripts/setup/install_dev_tools.py --skip-hooks`, tests
the automation boundaries, and runs the same `nexus-style` hook on a clean
checkout:

```sh
.venv/bin/python -m pre_commit run nexus-style --all-files --show-diff-on-failure
```

CI enables the full owned-source check and selects a trusted base from the PR
base revision, the push's previous revision, or the preceding commit for manual
dispatch. Local checks default to `HEAD`. The workflow retains the executed
style report on success or failure. Using a trusted base prevents a branch from
restoring old debt and presenting it as an unchanged exception.

The lightweight style job runs on every CI invocation, including documentation
changes and source paths outside the known module directories. Required static
analysis retains the code/workflow change conditions. For an initial push with
no previous commit, the comparison uses the fixed reviewed snapshot; a missing
nonzero comparison commit is fetched explicitly and must resolve successfully.
Previously owned source cannot be hidden by exclusions or renamed outside the
owned scope. The hooks use a framework-managed Python environment, so automatic
Git checks do not depend on shell activation; activation is convenient for the
manual commands above.

Commit hooks do not perform a full firmware build. The separate build/test
matrix and required static analysis remain in CI. A style pass establishes
only the executed mechanical checks; it does not establish compilation,
runtime correctness, MISRA compliance or physical qualification.

## Required static analysis

CI-002 replaces diagnostic jobs that ignored tool errors with a required static
analysis job. Both clang-tidy and cppcheck consume the actual compilation
database; the runner filters source ownership to Arch, Product, HAL, OSAL, framework, services,
platforms, boards and SoCs. Third-party source is excluded by path rather than
by hiding finding text. Duplicate source files with different compile commands
receive separate clang-tidy databases, retaining provider/configuration coverage.

Missing tools, failed version detection, malformed databases, absent source,
zero owned translation units, timeouts, tool crashes and diagnostic exits fail.
Reports preserve the tool version, commands and return codes on success/failure.
The workflow uploads reports even after a failed analysis. No empty report or
ignored stderr establishes MISRA compliance. Product qualification still needs
an agreed rule set, reviewed deviations and suitable tool evidence.

Process fixtures verify success/error/ownership rules and compiler-model import.
They model analyzer execution outcomes; they do not establish C correctness.
The first actual GitHub scan ran clang-tidy and cppcheck and failed. Its reports
exposed nullable HAL paths, ignored I/O results and an incomplete compiler model.
Those findings require source fixes and another actual scan; policy fixtures or
a successful tool launch do not establish that the repository passed.

Run the policy regressions with:

```sh
python -m unittest discover -s scripts/ci -p 'test_quality_tools.py' -v
```

Formatting and mechanical comment checks use the enforced gate above.
Complexity settings remain guidance unless backed by a separate executed gate.
Removing pretend-compliance output does not replace an actual product review.
Build, sanitizers, hardware budgets and artifact evidence use their separate
execution records and may not be inferred from these policy tests.

## Portable C correctness profile

The required clang-tidy profile enables clang-analyzer, bugprone and CERT
checks with findings treated as errors. The following advisory checks are
excluded explicitly after reviewing the first actual report:

| Check | Reason and separate control |
| --- | --- |
| `clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling` | Recommends optional Annex K `_s` APIs for every bounded `memcpy`/`memset`. These APIs are unavailable on the supported POSIX/newlib targets. Core buffer, null, allocation and lifetime analyzers remain enabled; length and overflow contracts also run with sanitizers. |
| `clang-analyzer-optin.performance.Padding` | Reorders public/device structures to reduce padding. Resource acceptance uses actual ELF/map budgets; an advisory layout suggestion does not establish a correctness defect. |
| `bugprone-easily-swappable-parameters` | Reports adjacent C buffer/length/timeout parameters without proving a swapped call. Call signatures and behavior remain subject to interface review and contract tests. |
| `bugprone-macro-parentheses` | Registration macros contain declaration types and aggregate initializers, where adding expression parentheses is not valid C. Arithmetic and evaluated macro arguments still require review; this exception does not waive compiler or analyzer findings. |
| `bugprone-reserved-identifier`, `cert-dcl37-c`, `cert-dcl51-cpp` | The report combines required POSIX feature-test macros, GNU linker-section identifiers and historical internal names. Renaming internal identifiers remains cleanup work; target-required compiler/OS identifiers cannot be removed. |

No check for null dereferences, bounds, integer narrowing/multiplication, ignored
results, leaks or use-after-free is disabled by this profile. It is a portable
correctness gate, not a claim that the entire CERT standard or MISRA is satisfied.
The repository `.clang-tidy` remains broader interactive development guidance;
the runner records the explicit required profile in each invocation.

## Reviewed host fixture inclusion

The GD32 UART, SPI and timebase fault fixtures deliberately include their actual
production C file in the same translation unit to inspect private factories and
state while using the fixture's register model and exact compiler definitions.
Only these three include lines carry `NOLINTNEXTLINE(bugprone-suspicious-include)`;
the corresponding production implementation, its included functions and all
other correctness checks remain in the required analysis scope. This reviewed
test arrangement is not allowed in product headers or applications, and does not
establish ARM instruction or hardware execution. The typed SPI vertical fixture
combines controller/platform models in its translation unit with the separately
compiled typed HAL core and exercises that core through its public APIs.
