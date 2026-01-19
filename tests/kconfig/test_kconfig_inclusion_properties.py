"""
\file            test_kconfig_inclusion_properties.py
\brief           Property-based tests for Kconfig file inclusion
\author          Nexus Team

Feature: kconfig-architecture-enhancement
Property 18: Kconfig file recursive inclusion completeness

Validates: Requirements 1.2, 1.3, 1.4
"""

import pytest
from hypothesis import given, strategies as st, settings, assume
from pathlib import Path
import tempfile
import os
import sys

# Add scripts directory to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / 'scripts' / 'kconfig'))

from validate_kconfig import KconfigValidator


# Strategy for generating valid Kconfig file content
@st.composite
def kconfig_content(draw, prefix=""):
    """Generate valid Kconfig file content"""
    lines = []

    # Add a config symbol with optional prefix for uniqueness
    symbol_name = draw(st.text(
        alphabet=st.characters(whitelist_categories=('Lu',), min_codepoint=65, max_codepoint=90),
        min_size=3,
        max_size=20
    ).map(lambda s: 'CONFIG_' + prefix + s.replace(' ', '_')))

    lines.append(f"config {symbol_name}")
    lines.append("    bool \"Test config\"")
    lines.append("    default y")

    return '\n'.join(lines)


@st.composite
def kconfig_with_source(draw, source_file):
    """Generate Kconfig content with source directive"""
    lines = []

    # Add a config symbol
    symbol_name = draw(st.text(
        alphabet=st.characters(whitelist_categories=('Lu',), min_codepoint=65, max_codepoint=90),
        min_size=3,
        max_size=20
    ).map(lambda s: 'CONFIG_' + s.replace(' ', '_')))

    lines.append(f"config {symbol_name}")
    lines.append("    bool \"Test config\"")
    lines.append("    default y")
    lines.append("")
    lines.append(f"source \"{source_file}\"")

    return '\n'.join(lines)


@settings(max_examples=100)
@given(content=kconfig_content())
def test_property_18_single_file_validation(content):
    """
    Property 18: Kconfig file recursive inclusion completeness

    For any valid Kconfig file, validation should succeed without errors.

    Validates: Requirements 1.2, 1.3, 1.4
    """
    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'

        # Write Kconfig file
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(kconfig_file)

        # Should succeed with no errors
        assert success, f"Validation failed with errors: {validator.errors}"
        assert len(validator.errors) == 0


@settings(max_examples=100)
@given(
    main_content=kconfig_content(),
    included_content=kconfig_content()
)
def test_property_18_source_file_exists(main_content, included_content):
    """
    Property 18: Kconfig file recursive inclusion completeness

    For any Kconfig file with source directive, if the source file exists,
    validation should succeed.

    Validates: Requirements 1.2, 1.3, 1.4
    """
    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)

        # Create included file
        included_file = tmpdir_path / 'included.kconfig'
        included_file.write_text(included_content)

        # Create main file with source directive
        main_file = tmpdir_path / 'Kconfig'
        main_content_with_source = main_content + '\n\nsource "included.kconfig"\n'
        main_file.write_text(main_content_with_source)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(main_file)

        # Should succeed with no errors
        assert success, f"Validation failed with errors: {validator.errors}"
        assert len(validator.errors) == 0


@settings(max_examples=100)
@given(content=kconfig_content())
def test_property_18_missing_source_file_warning(content):
    """
    Property 18: Kconfig file recursive inclusion completeness

    For any Kconfig file with source directive pointing to non-existent file,
    validation should produce a warning.

    Validates: Requirements 1.2, 1.3, 1.4
    """
    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)

        # Create main file with source directive to non-existent file
        main_file = tmpdir_path / 'Kconfig'
        main_content_with_source = content + '\n\nsource "nonexistent.kconfig"\n'
        main_file.write_text(main_content_with_source)

        # Validate
        validator = KconfigValidator(tmpdir)
        validator.validate_all(main_file)

        # Should have warning about missing file
        assert any('not found' in w.lower() for w in validator.warnings), \
            f"Expected warning about missing file, got: {validator.warnings}"


@settings(max_examples=100)
@given(
    level1_content=kconfig_content(prefix="L1_"),
    level2_content=kconfig_content(prefix="L2_"),
    level3_content=kconfig_content(prefix="L3_")
)
def test_property_18_recursive_inclusion(level1_content, level2_content, level3_content):
    """
    Property 18: Kconfig file recursive inclusion completeness

    For any Kconfig file hierarchy with multiple levels of inclusion,
    all files should be validated recursively.

    Validates: Requirements 1.2, 1.3, 1.4
    """
    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)

        # Create level 3 file
        level3_file = tmpdir_path / 'level3.kconfig'
        level3_file.write_text(level3_content)

        # Create level 2 file that includes level 3
        level2_file = tmpdir_path / 'level2.kconfig'
        level2_content_with_source = level2_content + '\n\nsource "level3.kconfig"\n'
        level2_file.write_text(level2_content_with_source)

        # Create level 1 file that includes level 2
        level1_file = tmpdir_path / 'Kconfig'
        level1_content_with_source = level1_content + '\n\nsource "level2.kconfig"\n'
        level1_file.write_text(level1_content_with_source)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(level1_file)

        # Should succeed with no errors
        assert success, f"Validation failed with errors: {validator.errors}"
        assert len(validator.errors) == 0

        # Should have parsed symbols from all three files
        assert len(validator.symbols) >= 3, \
            f"Expected at least 3 symbols from 3 files, got {len(validator.symbols)}"


@settings(max_examples=100)
@given(content=kconfig_content())
def test_property_18_rsource_relative_path(content):
    """
    Property 18: Kconfig file recursive inclusion completeness

    For any Kconfig file using rsource directive, the path should be
    resolved relative to the current file's directory.

    Validates: Requirements 1.2, 1.3, 1.4
    """
    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)

        # Create subdirectory
        subdir = tmpdir_path / 'subdir'
        subdir.mkdir()

        # Create included file in subdirectory
        included_file = subdir / 'included.kconfig'
        included_file.write_text(content)

        # Create main file in subdirectory with rsource
        main_file = subdir / 'Kconfig'
        main_content_with_rsource = content + '\n\nrsource "included.kconfig"\n'
        main_file.write_text(main_content_with_rsource)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(main_file)

        # Should succeed with no errors
        assert success, f"Validation failed with errors: {validator.errors}"
        assert len(validator.errors) == 0


def test_property_18_real_kconfig_structure():
    """
    Property 18: Kconfig file recursive inclusion completeness

    For the actual Nexus Kconfig structure, all included files should exist
    and be accessible.

    Validates: Requirements 1.2, 1.3, 1.4
    """
    # Get project root
    project_root = Path(__file__).parent.parent.parent
    kconfig_file = project_root / 'Kconfig'

    # Skip if Kconfig doesn't exist yet
    if not kconfig_file.exists():
        pytest.skip("Root Kconfig file not yet created")

    # Validate
    validator = KconfigValidator(project_root)
    success = validator.validate_all(kconfig_file)

    # Print any errors or warnings for debugging
    if validator.errors:
        print("\nValidation errors:")
        for error in validator.errors:
            print(f"  {error}")

    if validator.warnings:
        print("\nValidation warnings:")
        for warning in validator.warnings:
            print(f"  {warning}")

    # Should succeed with no errors
    assert success, f"Validation failed with {len(validator.errors)} error(s)"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
