# Current documentation build

Run from the repository root:

```sh
.venv/bin/python -m pip install -r docs/sphinx/requirements.txt
doxygen Doxyfile
.venv/bin/sphinx-build -n -W --keep-going -b html docs/sphinx docs/sphinx/_build/html/en
```

Doxygen selects only current public C headers. Sphinx imports that XML and
treats warnings as errors. English and Chinese locale builds use the same
current pages; a locale build does not claim completed translation. Chinese
design and delivery documents are in `docs/design` and `docs/delivery`.

Legacy HAL/OSAL/Kconfig tutorials and translation tools are retained under
`docs/archive/sphinx-before-next-generation`. Their examples describe
historical revisions. Existing format and Doxygen rules are preserved in
`development/coding_standards.rst`.
