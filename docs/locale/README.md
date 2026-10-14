# Canonical source and gettext translations

A page has one authored Markdown/RST source. Its `source_language` records that
source, not the selected site locale. C symbols, TOML keys, status constants,
CSV/JSON receipts and original evidence remain unchanged.

`tools/documentation/cli.py build --language all` uses the locked tools to extract
per-page gettext POT files before building `en` and `zh_CN`. The command receipt
locates the actual extraction directory. Do not generate a second hand-maintained
message list or translate a stale POT.

A PO catalog lives at `<language>/LC_MESSAGES/<docname>.po`. Its header includes:

- `Language: en` or `Language: zh_CN`;
- `Content-Type: text/plain; charset=UTF-8`;
- `X-Nexus-Source-SHA256`: SHA-256 of the exact canonical source bytes;
- `X-Nexus-Reviewed: true` only after independent semantic review.

Every nonheader extracted `msgid` must appear exactly once with a nonempty
`msgstr`. A PO must have the exact POT message set, no fuzzy entries, the current
canonical digest and a completed review. Changes to included prose also change
the actual required message set. The reviewed catalog staging directory is build
output; it never modifies authored PO files or commits generated MO files.

The page banner displays one of `original`, `reviewed`, `stale`, or `missing`.
`original` means the requested locale is the actual canonical language. A source
change or fuzzy catalog is `stale`. An absent, incomplete, unreviewed or unsupported
catalog is `missing`. Stale and missing catalogs are excluded from Sphinx, and
visible prose falls back to its source language. Each build clears its owned
reviewed-catalog stage, so an old compiled MO cannot preserve a revoked review.
The actual content region has
its actual HTML `lang` value. Locale selection cannot claim translated prose.

The release key paths comprise the home page, external SDK tutorial, asynchronous
I/O tutorial, assembly configuration, factory, ownership, support, qualification,
migration, Board porting and OS shutdown. Other pages may explicitly fall back to
original prose; their page status remains visible. The maintained bilingual terms
are in [terminology.csv](terminology.csv), with precise contracts linked per row.

The extension tests exercise actual gettext and both HTML locales, including a
Chinese source translated to English, an English source translated to Chinese,
source changes, fuzzy/incomplete review state and missing-catalog fallback.
A standalone single-language preview defaults to its one built locale. The full
CLI sets `NEXUS_DOCS_LANGUAGES=en,zh_CN`; an unknown, repeated or omitted requested
locale is rejected. Language switches appear only for declared actual outputs.
Documentation translation does not rerun firmware qualification or physical HIL.
