# Required quality gates

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

Repository formatting and complexity settings remain development guidance.
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
