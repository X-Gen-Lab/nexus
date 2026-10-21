# Source links with code labels

The Markdown source-link rewriter split inline code before matching complete
links. A label such as ``[`request.h`](../../core/include/nexus/core/request.h)``
therefore escaped revision binding. Whole-link and matching-code-span tokens now
retain the label markup while binding source, directory and archive targets.
Literal code examples, images and existing in-document links have regressions.

This pre-commit development packet binds two changed Python files and their
patch to parent `34955a3900f3314c8af8d0068bc154d1afc868ba`, tree
`c589c6db1c7dc69eaf3cc37ca8b953f617d69f6e`:

- Actual test-only RED ran all eight `DocumentationSourceTests` methods. The
  new regression produced 14 assertion failures before the implementation.
- The complete documentation suite then passed 336 tests, with zero
  failures/errors/skips/expected failures/unexpected successes.
- Fresh strict Doxygen/gettext/English/Chinese builds passed 127 pages and
  254 language records. No new catalog translation was required.
- An actual same-input comparison of the old and new rewriter covered the
  current 127 Markdown pages plus five recursive includes: all 132 output
  pairs were byte-identical, with input/source hashes stable during execution.
- The original independent semantic review still applies to these exact
  after-image hashes. The archive retains that review and both real phases.

`package.json` references one 765,764-byte archive containing 195 files /
3,724,435 bytes. Verification and actual full restoration matched all 234
inventory entries. The root separately verified the normally copied archive.
Raw commands, receipts, source patch, 127 fresh POTs and comparison records are
included; rendered HTML/fonts/doctrees/API XML and nested archives are excluded.

The accepted activation ordinary gate is a separate successor addendum in
`../usb-endpoint-activation-20261021`. This development packet does not establish
its own later complete-index acceptance, remote CI, formal qualification or HIL.
