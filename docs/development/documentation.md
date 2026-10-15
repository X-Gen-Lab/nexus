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

Follow the unified document commands documented by the current CLI. A source check,
strict bilingual build, real configuration/consumer execution, diagram audit, offline
package and actual published-site check are separate results. Record command exits
and exact inputs; never claim deployment from local HTML or hardware from a plan.

The [documentation ledger](../design/documentation-system-execution.csv) owns
implementation and validation exits. The migration map preserves historical paths;
old API redirects point to a version-aware migration or history page, not silently
to a different new function contract. Retain qualification records at their original
paths and identities.
