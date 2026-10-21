# USB endpoint activation development evidence

This packet binds the 32 changed implementation, test and current documentation
inputs to source HEAD `6b10c814c21d94b069f5690300152f904ea14d22`, tree
`a7109d16e669e0d015eb4f79978ca65cfbacce05`, plus the final source patch. Main
merge `8b27691c9a8ba4980ed2475eed6f43e46b9cc341` has the same tree and is only
the ARM layout comparator; it is not substituted for the actual build HEAD.

The optional activation face reuses the existing controller context and shared
read-only methods. Cold start activates only EP0. Explicit directional changes
prove geometry, NAK, DATA0 and masks; reset withdraws nonzero directions only
after genuine loan/FIFO drain. A latched raw reset rejects packet admission
before borrowing or writing. External Chapter 9 configuration remains external.

Actual development execution:

- Configuration RED: a three-test run with six expected getter subtest failures
  before implementing the getter; 24 complete configuration tests then passed.
- Runtime RED: seven complete programs, 126 cases and 24 expected failures with
  inert declarations. The first GREEN compilation failed on a test-header
  dependency; its original diagnostics remain separate from runtime results.
- Intermediate GREEN: 12 whole USB programs and 210 cases passed. The additional
  raw-reset regression then produced exactly three failed cases in 98 tests;
  after the admission fix, all 12 whole programs / 213 cases passed.
- The source-SDK header-closure regression failed before the required-file fix;
  all 29 source-SDK selection tests then passed. This is not the installed SDK
  matrix or clean-main SDK qualification.
- Six selected ARM links and two unselected controls passed. The retained
  activation face and shared ops occupy 8 and 4 read-only bytes; they point to
  the existing controller and executable method. Existing USB RAM layouts and
  static objects are unchanged. Each selected verification image adds 1,096
  Flash bytes, including verification-owner calls, with zero static RAM delta.
  These are whole-image measurements, not isolated driver cost or throughput.
- Strict Doxygen/gettext/English/Chinese builds passed 127 pages and 254 language
  records. The capability authority check passed. API and prose catalogs have
  independent semantic review; old translations remain preserved.

`package.json` references the compact raw archive: 863 files, 12,946,426 bytes.
`verified.json` records its verification; `restored.json` records actual full
restoration and equality of all 1,072 inventory entries. The root also verified
the normal copied archive after moving its storage path into this directory.
The archive retains raw commands, receipts, 34 Google XML reports, final source
patch, ARM artifacts, 127 fresh POTs, translation records and review metadata.

Generated HTML, fonts, doctrees and API XML are excluded from the source packet.
The separate full local recovery archive has digest
`291efc0e55f9afd0354bdfb642ef3c89671b6318593dfa34989de97e96767ca7`;
its verified full restore preceded retirement of the two exact generated docs
roots. Its body is not nested or committed here.

This is pre-commit development evidence. Ordinary complete-index and remote
checks are later phases and must bind their own accepted commit. Physical HIL,
complete USB class/configuration support and formal clean-main qualification
are not established by this packet.

## Accepted activation ordinary successor

Commit `34955a3900f3314c8af8d0068bc154d1afc868ba`, tree
`c589c6db1c7dc69eaf3cc37ca8b953f617d69f6e`, passed the unchanged ordinary
and commit-message gates. All 43 frozen Git blobs and the original 32-source
subset match. The actual build used dirty precommit HEAD
`6b10c814c21d94b069f5690300152f904ea14d22` and the frozen index tree;
original build receipts retain that identity. This does not claim a clean build
of the newly created commit.

The completed master contains 706 fresh commands: 350 Google executables /
6,323 passes and 2,956 tool passes (1,368 testing, 974 configuration, 485 CI,
129 workflows), with zero failures/errors/skips/disabled cases. The IO audit
retains its unproven scopes; these results do not establish formal or physical
qualification.

`accepted-34955-package.json` references the single compact successor archive.
Verification and actual full restoration matched all 3,107 inventory entries:
2,391 files / 19,280,672 bytes. The archive is 1,564,654 bytes and excludes
source, whole builds, vendors and older archive bodies. The successor is stored
by the later documentation-link increment, separate from its own acceptance.
