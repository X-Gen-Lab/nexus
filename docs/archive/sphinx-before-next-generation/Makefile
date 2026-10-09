# Nexus Documentation Makefile
# Makefile for Sphinx documentation with i18n support
#
# Recommended: Use Python build script for better cross-platform support
#   python build_docs.py --help
#
# Usage:
#   make html          - Build English documentation
#   make html-zh_CN    - Build Chinese documentation
#   make html-all      - Build all languages
#   make gettext       - Extract translatable strings
#   make update-po     - Update .po files from .pot
#   make clean         - Remove build directory

SPHINXOPTS    ?=
SPHINXBUILD   ?= sphinx-build
SPHINXINTL    ?= sphinx-intl
SOURCEDIR     = .
BUILDDIR      = _build
LANGUAGES     = en zh_CN
PYTHON        ?= python

# Default target
.PHONY: help
help:
	@echo "╔════════════════════════════════════════════════════════════╗"
	@echo "║   Nexus Documentation Build System (i18n)                 ║"
	@echo "╚════════════════════════════════════════════════════════════╝"
	@echo ""
	@echo "📚 Build targets:"
	@echo "  html          Build English documentation"
	@echo "  html-zh_CN    Build Chinese documentation"
	@echo "  html-all      Build all languages"
	@echo "  full          Clean, run Doxygen, build all"
	@echo ""
	@echo "🌍 Translation targets:"
	@echo "  gettext       Extract translatable strings to .pot files"
	@echo "  update-po     Update .po files from .pot files"
	@echo "  init-po       Initialize new language (LANG=xx)"
	@echo "  stats         Show translation statistics"
	@echo "  validate      Validate translation files"
	@echo "  auto-trans    Auto-translate common terms"
	@echo ""
	@echo "🔧 Other targets:"
	@echo "  clean         Remove build directory"
	@echo "  serve         Serve documentation locally (port 8000)"
	@echo "  doxygen       Run Doxygen to generate API docs"
	@echo "  linkcheck     Check for broken links"
	@echo "  watch         Auto-rebuild on file changes"
	@echo ""
	@echo "💡 Recommended: Use Python script for better experience"
	@echo "  $(PYTHON) build_docs.py --help"

# Build English (default)
.PHONY: html
html:
	@echo "Building English documentation..."
	$(SPHINXBUILD) -b html $(SPHINXOPTS) "$(SOURCEDIR)" "$(BUILDDIR)/html/en"
	@echo "English docs built: $(BUILDDIR)/html/en/index.html"

# Build Chinese
.PHONY: html-zh_CN
html-zh_CN:
	@echo "Building Chinese documentation..."
	$(SPHINXBUILD) -b html -D language=zh_CN $(SPHINXOPTS) "$(SOURCEDIR)" "$(BUILDDIR)/html/zh_CN"
	@echo "Chinese docs built: $(BUILDDIR)/html/zh_CN/index.html"

# Build all languages
.PHONY: html-all
html-all: html html-zh_CN index-page
	@echo ""
	@echo "All documentation built successfully!"
	@echo "  Language selector: $(BUILDDIR)/html/index.html"

# Create language selection index page
.PHONY: index-page
index-page:
	@python build_docs.py --lang en 2>/dev/null || true
	@python -c "exec(open('build_docs.py').read().split('def create_index_page')[1].split('def ')[0])" 2>/dev/null || \
	echo '<!DOCTYPE html><html><head><meta http-equiv="refresh" content="0;url=en/index.html"></head></html>' > $(BUILDDIR)/html/index.html

# Extract translatable strings
.PHONY: gettext
gettext:
	@echo "Extracting translatable strings..."
	$(SPHINXBUILD) -b gettext $(SPHINXOPTS) "$(SOURCEDIR)" "$(BUILDDIR)/gettext"
	@echo "POT files created in $(BUILDDIR)/gettext/"

# Update .po files from .pot files
.PHONY: update-po
update-po: gettext
	@echo "Updating .po files..."
	@for lang in $(filter-out en,$(LANGUAGES)); do \
		echo "  Updating $$lang..."; \
		$(SPHINXINTL) update -p $(BUILDDIR)/gettext -l $$lang; \
	done
	@echo "Translation files updated in locale/"

# Initialize new language
.PHONY: init-po
init-po: gettext
ifndef LANG
	$(error LANG is not set. Usage: make init-po LANG=ja)
endif
	@echo "Initializing translation for $(LANG)..."
	$(SPHINXINTL) update -p $(BUILDDIR)/gettext -l $(LANG)
	@echo "Translation files created in locale/$(LANG)/LC_MESSAGES/"

# Clean build directory
.PHONY: clean
clean:
	rm -rf $(BUILDDIR)
	@echo "Build directory cleaned."

# Serve documentation locally
.PHONY: serve
serve: html-all
	@echo "Serving documentation at http://localhost:8000"
	@cd $(BUILDDIR)/html && python -m http.server 8000

# Rebuild everything from scratch
.PHONY: rebuild
rebuild: clean html-all

# Translation statistics
.PHONY: stats
stats:
	@$(PYTHON) translate_helper.py zh_CN --stats

# Validate translations
.PHONY: validate
validate:
	@$(PYTHON) translate_helper.py zh_CN --validate

# Auto-translate common terms
.PHONY: auto-trans
auto-trans:
	@$(PYTHON) translate_helper.py zh_CN --auto-translate

# Run Doxygen
.PHONY: doxygen
doxygen:
	@echo "Running Doxygen..."
	@cd ../.. && doxygen Doxyfile
	@echo "✓ Doxygen API documentation generated"

# Check for broken links
.PHONY: linkcheck
linkcheck:
	@echo "Checking for broken links..."
	$(SPHINXBUILD) -b linkcheck $(SPHINXOPTS) "$(SOURCEDIR)" "$(BUILDDIR)/linkcheck"
	@echo "✓ Link check complete. See $(BUILDDIR)/linkcheck/output.txt"

# Full build: clean, doxygen, build all
.PHONY: full
full: clean doxygen html-all
	@echo ""
	@echo "╔════════════════════════════════════════════════════════════╗"
	@echo "║   ✓ Full documentation build complete!                    ║"
	@echo "╚════════════════════════════════════════════════════════════╝"

# Watch for changes (requires sphinx-autobuild)
.PHONY: watch
watch:
	@echo "Watching for changes... (Press Ctrl+C to stop)"
	@sphinx-autobuild $(SOURCEDIR) $(BUILDDIR)/html/en --port 8000

# Quick build using Python script (recommended)
.PHONY: quick
quick:
	@$(PYTHON) build_docs.py

# Quick serve using Python script
.PHONY: quick-serve
quick-serve:
	@$(PYTHON) build_docs.py --serve
