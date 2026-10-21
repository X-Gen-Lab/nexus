# Canonical H743 Flash input: development evidence

This packet records the migration of the existing H743 high-peripheral Flash
assembly into `tools/configure/assemblies/h743-high-peripherals.toml` as its
single authored authority. Consumer policy v11 retains 50 roots and 53 images;
the policy change does not establish that the complete image plan was executed.
No runtime Flash driver change is included in this increment.

The original phase starts from commit
`56a66e05014d760fbbd8d3e2e3b5909e86e80da6`, tree
`6aff81836de5b85ec71883e36dcdd2c6d5b262dd`, with the canonical increment applied
as dirty development inputs. `source-capture.json` freezes all 18 increment
entries before root temporarily stashes them: 17 regular source files and one
deleted prior assembly path. It records the actual source-doc link-label
successor rather than relabeling the earlier author manifest.

The real pre-change test observation executed four cases, with three failures,
zero errors and zero skips. Its three new-method identities and actual `red-v3`
command, attempt and streams are preserved. Root subsequently executed four
new/control cases successfully, followed by all 211 cases in the five affected
complete suites: configure 37, tools 66, SDK selection 29, release 58 and
qualification guards 21. The first focused runner accidentally stopped after
its successful four-case command; its record is retained as a runner-control
error, not a functional RED or an affected-suite pass.

The normal canonical ARM graph configured, built and passed linked-provider
retention. Its public Flash face retains the 16-byte read-only operations table
and all four methods. The ELF, BIN, map, resource report, automatic Flash RAM
audit, provider-retention report, compile commands, exact generated input
bundle and command provenance are retained. The original execution record
contains before/after source identities; those identities are equal. The
6,769 source bodies, vendor payload and complete build tree are not included.
The emitted image has 28,296 loaded Flash bytes and 6,704 reserved AXI SRAM
bytes, including the 2,048-byte MSP reservation. These values describe this
exact development image; they are not an isolated implementation cost or
measured timing result.

The first ARM runner failed while identifying an empty historical file before
any configure or compile command. Its setup-failure record and original/fixed
runner scripts are preserved separately from the successful production graph.

Four minimal reviewed catalogs were integrated after fresh real-primary POT
extraction. Strict Doxygen/gettext/English/Chinese builds passed all 127 pages
and 254 language records. The translation review and actual command streams
are retained under `documentation/`. No parser implementation was changed.

The single archive also retains the accepted ordinary R2 for the independent
TSan-model repair at `ed4f479f055fc8c52b9d808bde21f2c6bfdbd157`, tree
`f150ff5686a34106c6a09c68901a59c6cb474b1a`: 6,288 Google cases in 348
executables and 2,946 tool cases, all zero failures/errors/skips. Its five
frozen Git blobs, 702 actual commands, receipts and XML were independently
audited. The environmental interruption in its first ordinary attempt remains
explicitly unaccepted and unreused. Its original collected payload was moved
once into `accepted-tsan-ed4/` before sealing, without hardlinks or new bytes;
the original collection manifests and relocation map remain intact.

This is a pre-gate evidence snapshot. The H743 successor's complete ordinary
commit gate was not yet executed when this archive was sealed; its actual
commit and remote checks must be assessed separately. Earlier source-bound
ARM and focused runs are development evidence, including the phase before
the unrelated host-only TSan repair. No installed-SDK matrix, clean-main
formal candidate, remote acceptance, physical MCU execution or complete
IO-24 hardware exit is established by this archive.

[package.json](package.json) binds the single compact raw archive and member
manifest; [verified.json](verified.json) records byte verification. No previous
F407 archive or whole source/vendor/build tree is duplicated here. The living
README and relocated package locator remain outside the immutable raw payload.

## Accepted `430b0aea` successor

The [accepted collection](accepted-430-COLLECTION.md) separately retains
the actual later complete-index gate: 6,288 Google cases and 2,951 tool cases.
The original `71d1...` archive above remains unchanged and describes its earlier
archival phase; neither archive establishes the later USB increment or HIL.
