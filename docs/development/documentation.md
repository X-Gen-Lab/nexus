---
id: docs-development-documentation
type: how-to
audience: maintainer
source_language: en
---

(docs-development-documentation)=
# Maintain clear and verifiable documentation

Use the [content and diagram standard](../design/documentation-standards.md) and
[five page templates](../design/documentation-templates.md) for every new or changed
page. Keep one authoritative contract and one canonical prose source. Current
implementation, target design and exact reference configurations remain distinct.

## Write around a reader task

State the result, prerequisites and limit at the start. Explain who acts, on what
input, in which context, and what observable result or failure follows. Use stable
terms and explicit units. Keep raw hashes/logs in evidence links rather than the
operation narrative. Do not replace scope with claims such as complete support.

Tutorials provide a complete maintained fixture or pinned external consumer,
actual commands, expected results and cleanup. Use literalinclude for source-owned
inputs. Fragments state why they are incomplete. Ordinary document builds do not
execute arbitrary Markdown commands, fetch latest examples or operate hardware.

## Review diagrams and translations

A diagram has an ID, scope, caption, alt text, editable source and local SVG.
Label arrow meanings, optional paths, owners and failure responsibility. Review
against actual code/contract, then render and inspect text, line crossings, zoom,
print and offline resources. Successful rendering alone does not prove semantics.

English canonical pages use gettext PO for Chinese; canonical Chinese contracts
keep their source language. A translation tracks the actual canonical source digest,
actual extracted messages and independent review. Changed source, fuzzy or missing
messages display the true status instead of pretending a complete translation.
Do not translate C symbols, configuration keys, result codes or original receipts.
Use the [reviewed bilingual terminology](../locale/terminology.csv) for these shared
terms; its authority links preserve the single contract.

## Apply the correct acceptance boundary

Follow the current CLI commands for source checks, strict bilingual builds and
complete documentation tests. Record actual exit codes and inputs. A local HTML
build does not establish deployment, firmware acceptance or physical HIL.

Current source contains maintained contracts and reader guides. Historical plans,
ledgers and reports remain in fixed Git revisions with their original identities.
Update current navigation, source links and translations together; removed tools
must not remain as runnable commands. The platform has no separate documentation
archive recovery or candidate promotion engine.
