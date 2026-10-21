# Bounded I3C core development evidence

This record binds the optional, allocation-free C11 `Nexus::I3CController`
software core and its external aggregate hardware-port contract. It does not
supply a physical controller provider, H563 SoC/Board/factory, full-family support,
clean software candidate, reproducibility result or physical qualification.
Physical HIL is `not_executed`; the original IO-38 full exit remains open.

`development.tar.gz` is the single retained raw record. `members.json` gives
member sizes and SHA-256 identities; `summary.json` gives the archive identity,
source freeze and actual phase results. Extract the archive into an empty local
directory to inspect commands, actual return codes, fresh Google XML, raw stdout,
source hashes, independent review, ARM ELF/map/object bytes and pinned source
facts. The archive retains the byte-identical initial failing C scaffold and
its actually executed RED binary. Framework binaries and repeated intermediate
build trees are excluded; the recorded hashes identify the existing pinned
GoogleTest/GoogleMock libraries used read-only from the main Native build.
`notices/googletest-LICENSE` covers the statically linked RED test binary;
`notices/nexus-LICENSE` covers the retained Nexus source.

## Executed development scope

| Scope | Actual result and limit |
| --- | --- |
| Initial actual production-C scaffold | 32 Google cases, 32 failures; C11/C++ compile and link passed, execution returned 1 |
| DAA observation order regression | 34 cases, 2 failures before correction; 34 passed after correction |
| Address-preserving final stop regression | 34 cases, 1 failure before correction; 37 passed after correction and added boundary checks |
| BCR-dependent IBI shape regression | 38 cases, 1 failure before correction; 38 passed after correction |
| Faulted final stop regression | 39 cases, 1 failure before correction; 39 passed after correction |
| Definitive frozen C core and API | 40 passed, zero failures/skips; includes independent GETMRL BCR-dependent 2/3-byte boundary coverage |
| Source-SDK selection/consumer regression | Actual RED then GREEN: 2 tests with Native/F407/GD470 consumer subcases |
| Public Cortex-M33 Runtime | GNU 14.3.1, C11 compile/link, 12 I3C APIs retained, no undefined symbols; text 4020 B, data 0 B, BSS 240 B |
| Private STM32H563 reference object | Pinned official CMSIS/HAL-LL declarations compile in the same Runtime graph; object excluded from ELF/map |
| Independent lifecycle/protocol review | Final exact core/API bytes approved; resolved IBI advertisement/MDB shape and faulted-stop findings |

Every compiler/configure invocation acquired `/tmp/nexus-platform-compiler.lock`
and used one build job. The focused Google run compiled the actual production
C source and mocked only external hardware and clock leaves. Firmware remains
C11 and never links the host test framework. The tiny Runtime source view used
the unchanged production `Runtime.cmake` and component graph, with only the
pending component registration and SDK integration files overlaid; it is not an
independent source build graph. The archive records those overlays explicitly.

After integration, the ordinary `tools/testing/test_i3c_sdk.py` suite protects SDK
required payload/selection and installed consumer generation. The normal
`tests/google/i3c_controller.cmake` target joins all Google contract discovery.
Root-owned complete-index gates and installed source-SDK rebuild remain separate
integration evidence and must not inherit this development result.

## Pinned reference facts and source boundary

Archive members `research/h563-facts/FACTS.md` and `source-manifest.json` retain
the original read-only research and exact official URLs, revisions, byte counts
and hashes. Their research-phase status text is historical; later executed
software validation is recorded separately above. Selected official headers
actually used by the private H563 object and their original license notices are
retained as reference inputs, without modifying a production vendor tree.
The full RM0481 was not downloaded; a reviewed hardware quiescence sequence is
still absent. PDF reference bytes are identified by their original hashes/URLs
and are not redistributed here.

ES0565 Rev 8 requires SDA pull-up preparation for 1 ms and `NOARBH=0`.
AN5879 establishes two-word control/status FIFOs, eight-byte TX/RX FIFOs and at
most four total IBI data bytes including MDB when advertised. H563 has at most
four IBI acknowledgement slots. Header compilation does not establish these
electrical timings or a true wire/FIFO/IRQ/all-target-reset proof. No currently
supported Nexus chip gains I3C from this reference, and there is no physical
provider stub presented as an implementation.

The authoritative API and lifecycle contract is
[the current I3C design page](../../../docs/design/i3c-controller.md).
