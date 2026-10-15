# Google plan path and serial descriptor regressions

The Google runner's noncatalogued executable plan entries had the same lexical
path gap found in matrix capture. Three real temporary filesystem cases first
failed against the production main loop: a regular `..` escape, a file symlink,
and a parent directory symlink. One shared `checked_binary` helper now checks
regular bytes, canonical selected-build containment and every symlink ancestor
before subprocess execution. Matrix capture reuses it. All 59 existing and
new gate, report, staged-index, matrix and orchestration tests passed with the
three owned inputs unchanged. Orchestration fixture XML is synthetic tool
input and does not establish provider execution.

Serial trace construction leaked the raw descriptor when `fdopen` failed;
the JSON writer had the same ownership gap. Trace close also abandoned later
streams after flush/fsync failed, and a cleanup error could replace the first
failure. Four initial and six expanded regressions failed against the actual
production code. They use real `os.open`/`os.fstat` descriptors with injected
external I/O failures, clean up leaked test handles, and verify EBADF after
cleanup. The minimum helper transfers descriptor ownership only after a
successful conversion; close attempts every retained stream and preserves
the first exception. The full 28-case serial suite, including real host PTY
echo and failure paths, then passed with both inputs unchanged.

`manifest.json` binds exact raw logs, commands and RED/final source capsules.
These tests add no provider runner, target registry, MCU state or worker. The
original HIL preparation freeze and earlier property archives remain historical
bytes. This later source still requires root's final complete clean gate and
source SDK qualification. No physical serial port, board, timing or electrical
qualification was performed. A failing OS close cannot be promoted to a
reclamation guarantee; potentially recycled descriptor integers are not retried.
