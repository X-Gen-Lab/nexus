---
id: reference.documentation_tools
type: reference
audience: maintainer
source_language: en
---

# Documentation tools and preparation

Use this reference to prepare one maintained documentation environment. The
qualified build tuple is Linux x86_64 with CPython 3.12. The local preparation
uses CPython 3.12.14; CI selects the same patch version. Documentation tool
qualification does not grant firmware, hardware or product qualification.

First prepare the [fixed source and recursive dependencies](../guides/source-checkout.md).
The manifest and tool lock must come from the same selected source revision.

## The authoritative closure

`dependencies/docs-tools.lock.json` owns archive identities, target constraints,
direct requirements and license provenance. The complete Python dependency set
is also expressed in `dependencies/docs-python-linux-cp312.txt`, using exact
versions and SHA-256 hashes. `docs/sphinx/requirements.txt` references that file;
it does not define another resolver input.

| Tool or resource | Maintained identity | Role |
|---|---|---|
| Sphinx | 7.4.7 | Strict HTML and gettext builders |
| MyST parser | 4.0.1 | Markdown directives, roles and navigation |
| Breathe | 4.35.0 | Public API from fresh Doxygen XML |
| Docutils | 0.21.2 | Structured document processing |
| Alabaster | 0.7.16 | Local HTML theme assets |
| polib | 1.2.0 | Translation catalog validation |
| Python closure | 28 individually hashed wheels | All runtime transitive packages |
| Doxygen | Official Linux 1.14.0 archive | Public header XML and HTML |
| Graphviz | Debian 2.42.4-3 archives | Reviewed DOT diagrams |
| Noto CJK | Debian 20240730+repack1-1 archive | Exact CJK glyphs and metrics |
| Native closure | 46 individually hashed Debian archives | Loader, libraries, renderers and font data |

The native closure includes its libc loader, C/C++ runtime libraries, Graphviz
core/dot/Pango plugins, fontconfig and each library actually linked by those
executables and renderers. Preparation executes the locked loader's `--list`
operation and rejects paths outside the prefix. It removes unsupported renderers
instead of discovering host GTK, GD or Ghostscript installations. This tool set
supports the maintained DOT/SVG/Cairo diagram path, not every Graphviz renderer.

Linux kernel, the CPython/venv bootstrap, `dpkg-deb` and a POSIX shell are explicit
host prerequisites. Their observed values belong to the preparation receipt.
This lock does not claim a reproducible host image or a bitwise identical kernel.

## Prepare a fresh prefix

Run from the repository root, with CPython 3.12 available:

```sh
python3.12 tools/documentation/install.py \
    --prefix build/docs-tools --cache build/docs-tool-cache
```

Preparation may download archives over HTTPS. It verifies the entire archive
closure before creating the prefix. Wheel installation then uses `--no-index`,
`--find-links` and `--require-hashes`; no transitive package is left for a fresh
online resolution. Native packages are extracted locally without `sudo` or
system package installation.

For an independent installation from a previously prepared cache:

```sh
python3.12 tools/documentation/install.py \
    --prefix build/docs-tools-restored --cache build/docs-tool-cache --offline
```

The expected result is exit code zero and `installation.json` in the fresh
prefix. It records the lock digest, observed tool versions, kernel, resolved ELF
closure, archive digests, installed file digests and copied license digests.
The installed `bin/fc-match` must resolve `Noto Sans CJK SC` from the locked font
package. A missing archive, changed byte, failed command or mismatched font fails
preparation and leaves no success prefix.

An existing prefix is rejected. Choose a fresh path for an upgrade; do not
silently mutate an installation already referenced by build evidence. Archive
cache corruption also fails rather than being silently replaced by a download.

## Use the single documentation CLI

Run the maintained CLI with the prepared Python and explicit tool prefix:

```sh
build/docs-tools/venv/bin/python tools/dev/dev.py docs doctor \
    --tool-prefix build/docs-tools
build/docs-tools/venv/bin/python tools/dev/dev.py docs check \
    --report build/docs-check.json
build/docs-tools/venv/bin/python tools/dev/dev.py docs build \
    --tool-prefix build/docs-tools --language all --output build/documentation
build/docs-tools/venv/bin/python tools/dev/dev.py docs package \
    --build build/documentation --output ../nexus-documentation-packages
```

`install.environment(prefix)` returns a verified subprocess environment with the
locked tool executables first on `PATH`. `install.diagnose(prefix)` rejects a
changed lock, changed installed file or changed license before reporting tool
identity. Neither operation installs or downloads tools. Builds must generate
fresh XML and HTML; a former successful output cannot rescue a failed command.

The `scripts/tools/docs.py`, `.sh`, `.ps1` and `.bat` entries only forward to this
CLI. Legacy `all`, `doxygen` and `sphinx` targets all select the same complete
strict build. Removed generation modifiers are rejected; wrappers cannot create
a fallback Doxyfile or Sphinx configuration. Windows and macOS wrappers preserve
CLI forwarding, while a formal build still requires the maintained Linux tuple.

## Replay the API mobile-layout regression

This optional diagnostic needs an existing Chromium and Playwright in addition
to the documented Sphinx tuple. It never installs tools. Use a separate prepared
browser test environment; the formal documentation build and default stdlib test
suite do not import Playwright.

Run from the repository root, supplying the existing browser executable:

```sh
python tools/documentation/check_browser_layout.py \
    --chromium /path/to/chromium --report build/quality/api-layout.json
```

The helper builds a temporary English/Chinese C API fixture from the production
theme. At 390 CSS pixels it checks long signatures, unchanged symbols and anchors,
complete code and table content with internal scrolling, and an unclipped page.
Missing tools or failed checks return nonzero. A successful receipt qualifies
only that fixture and observed browser; full-site responsive acceptance and
archive verification remain separate.

## Search the current site

The English and Chinese search pages use their own local Sphinx index and
current-language metadata. The Chinese tokenizer has no external dictionary
dependency and accepts Han character sequences and Latin identifiers. Separate
keywords with whitespace for combined queries, such as `借用 排空` or `借用 HAL`.

Results show the version, content type, source and actual language, and body
excerpts. Search assets load only on the search page. Archive links and original
qualification records retain their frozen source and language scope.

## CI permission boundary

`docs-build.yml` has only `contents: read`. It prepares tools explicitly, runs
source checks, builds both languages, packages offline output and records observed
job identity. A failed build has no successful documentation upload.
The package producer writes outside the source checkout. CI then copies the
verified result to fresh ignored upload staging, compares the archive digest and
verifies the copied manifest and payload before the existing job-identity gate.
The staging copy does not rebuild or replace the original package.

`docs-publish.yml` runs only for the reviewed `docs-delivery` branch. Its read-only
job verifies the prepared public bytes and publication receipt before uploading a
Pages artifact. The separate deployment job alone has Pages and OIDC write
permissions and uses the `nexus-documentation` environment. It never checks out
or executes external pull-request source code. Deployment branch rules and
actual remote execution remain independently observable repository state.

## License and upgrade handling

Each native archive carries its Debian copyright files. The locked `base-files`
archive supplies every referenced complete text in `licenses/common-licenses/`;
preparation rejects a missing shared license reference. Wheel license files are
copied from the verified wheel; the five upstream wheels which omit license text
use checked-in texts extracted from their exact verified source distributions.
The lock records source archive digest, member and extracted text digest. The
Doxygen archive carries its complete license text. Preparation copies these
texts into `prefix/licenses/`; packaging retains the applicable license payload.

To upgrade, review upstream changes and licenses, resolve one new exact closure,
update both lock and hash requirements, execute a fresh online preparation and
an independent offline preparation, then run strict bilingual build, diagram
freshness, package verification and restoration. Retain raw failures and results;
an updated version string alone is insufficient acceptance evidence.
