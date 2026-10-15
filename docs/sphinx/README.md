# Documentation build entry

Use the [source checkout guide](../guides/source-checkout.md) to obtain the exact
documentation revision and initialize its pinned dependencies. The canonical
prose source root is `docs/`; this directory contains only Sphinx configuration
and retained compatibility wrappers. Building `docs/sphinx` directly produces a
different, incomplete page set.

Prepare the hash-locked Linux x86_64 documentation tuple from the repository
root, with CPython 3.12 available:

```sh
python3.12 tools/documentation/install.py \
    --prefix build/docs-tools --cache build/docs-tool-cache
build/docs-tools/venv/bin/python tools/dev/dev.py docs doctor \
    --tool-prefix build/docs-tools
build/docs-tools/venv/bin/python tools/dev/dev.py docs check \
    --report build/docs-check.json
build/docs-tools/venv/bin/python tools/dev/dev.py docs build \
    --tool-prefix build/docs-tools --language all --output build/documentation
build/docs-tools/venv/bin/python tools/dev/dev.py docs package \
    --build build/documentation --output ../nexus-documentation-packages
```

Use a fresh tool prefix and package destination. The unified CLI generates fresh
public-header Doxygen XML, extracts gettext messages, checks reviewed catalogs,
and builds English and Chinese HTML with warnings treated as errors. It verifies
diagrams, local links, identities and the offline payload. Success requires exit
code zero, a passed `documentation-build.json` and verified package receipts;
existing HTML cannot rescue a failed build. Keep receipts before removing only
outputs you created.

The [documentation tool reference](../reference/documentation-tools.md) owns
tool identities, preparation, offline restoration, permissions and upgrade
requirements. [Documentation maintenance](../development/documentation.md)
owns content and review practice. A successful documentation build establishes
neither firmware qualification nor physical board acceptance.

Legacy HAL/OSAL/Kconfig tutorials and translation tools are retained under
`docs/archive/sphinx-before-next-generation`. Their examples describe
historical revisions. Existing format and Doxygen rules are preserved in the
[coding standard](development/coding_standards.rst).
