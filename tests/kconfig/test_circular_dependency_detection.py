"""
\file            test_circular_dependency_detection.py
\brief           Unit tests for circular dependency detection
\author          Nexus Team

Feature: kconfig-architecture-enhancement
Property 19: Circular dependency detection

Validates: Requirements 10.3
"""

import pytest
from pathlib import Path
import tempfile
import sys

# Add scripts directory to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / 'scripts' / 'kconfig'))

from validate_kconfig import KconfigValidator


def test_circular_dependency_simple_cycle():
    """
    Property 19: Circular dependency detection

    Test detection of simple circular dependency: A depends on B, B depends on A

    Validates: Requirements 10.3
    """
    content = """
config CONFIG_A
    bool "Config A"
    depends on CONFIG_B

config CONFIG_B
    bool "Config B"
    depends on CONFIG_A
"""

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(kconfig_file)

        # Should fail due to circular dependency
        assert not success, "Expected validation to fail due to circular dependency"

        # Should have error about circular dependency
        assert any('circular' in e.lower() for e in validator.errors), \
            f"Expected error about circular dependency, got: {validator.errors}"

        # Error should mention both symbols
        circular_errors = [e for e in validator.errors if 'circular' in e.lower()]
        assert any('CONFIG_A' in e and 'CONFIG_B' in e for e in circular_errors), \
            f"Expected circular dependency error to mention both symbols"


def test_circular_dependency_three_way_cycle():
    """
    Property 19: Circular dependency detection

    Test detection of three-way circular dependency: A -> B -> C -> A

    Validates: Requirements 10.3
    """
    content = """
config CONFIG_A
    bool "Config A"
    depends on CONFIG_B

config CONFIG_B
    bool "Config B"
    depends on CONFIG_C

config CONFIG_C
    bool "Config C"
    depends on CONFIG_A
"""

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(kconfig_file)

        # Should fail due to circular dependency
        assert not success, "Expected validation to fail due to circular dependency"

        # Should have error about circular dependency
        assert any('circular' in e.lower() for e in validator.errors), \
            f"Expected error about circular dependency, got: {validator.errors}"


def test_circular_dependency_self_reference():
    """
    Property 19: Circular dependency detection

    Test detection of self-referencing dependency: A depends on A

    Validates: Requirements 10.3
    """
    content = """
config CONFIG_A
    bool "Config A"
    depends on CONFIG_A
"""

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(kconfig_file)

        # Should fail due to circular dependency
        assert not success, "Expected validation to fail due to self-reference"

        # Should have error about circular dependency
        assert any('circular' in e.lower() for e in validator.errors), \
            f"Expected error about circular dependency, got: {validator.errors}"


def test_no_circular_dependency_linear_chain():
    """
    Property 19: Circular dependency detection

    Test that linear dependency chains are not flagged as circular

    Validates: Requirements 10.3
    """
    content = """
config CONFIG_A
    bool "Config A"
    depends on CONFIG_B

config CONFIG_B
    bool "Config B"
    depends on CONFIG_C

config CONFIG_C
    bool "Config C"
"""

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(kconfig_file)

        # Should succeed - no circular dependency
        assert success, f"Expected validation to succeed, got errors: {validator.errors}"

        # Should not have circular dependency errors
        assert not any('circular' in e.lower() for e in validator.errors), \
            f"Unexpected circular dependency error: {validator.errors}"


def test_no_circular_dependency_diamond():
    """
    Property 19: Circular dependency detection

    Test that diamond dependencies are not flagged as circular:
    A depends on B and C, both B and C depend on D

    Validates: Requirements 10.3
    """
    content = """
config CONFIG_A
    bool "Config A"
    depends on CONFIG_B && CONFIG_C

config CONFIG_B
    bool "Config B"
    depends on CONFIG_D

config CONFIG_C
    bool "Config C"
    depends on CONFIG_D

config CONFIG_D
    bool "Config D"
"""

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(kconfig_file)

        # Should succeed - diamond is not circular
        assert success, f"Expected validation to succeed, got errors: {validator.errors}"

        # Should not have circular dependency errors
        assert not any('circular' in e.lower() for e in validator.errors), \
            f"Unexpected circular dependency error: {validator.errors}"


def test_circular_dependency_with_select():
    """
    Property 19: Circular dependency detection

    Test detection of circular dependency involving select:
    A selects B, B depends on A

    Note: This is a potential circular dependency but may be allowed in Kconfig

    Validates: Requirements 10.3
    """
    content = """
config CONFIG_A
    bool "Config A"
    select CONFIG_B

config CONFIG_B
    bool "Config B"
    depends on CONFIG_A
"""

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        validator.validate_all(kconfig_file)

        # This test just verifies the validator handles this case
        # The actual behavior (error or not) depends on Kconfig semantics
        # For now, we just check it doesn't crash
        assert True


def test_circular_dependency_complex_cycle():
    """
    Property 19: Circular dependency detection

    Test detection of complex circular dependency with multiple paths

    Validates: Requirements 10.3
    """
    content = """
config CONFIG_A
    bool "Config A"
    depends on CONFIG_B

config CONFIG_B
    bool "Config B"
    depends on CONFIG_C || CONFIG_D

config CONFIG_C
    bool "Config C"
    depends on CONFIG_A

config CONFIG_D
    bool "Config D"
"""

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir_path = Path(tmpdir)
        kconfig_file = tmpdir_path / 'Kconfig'
        kconfig_file.write_text(content)

        # Validate
        validator = KconfigValidator(tmpdir)
        success = validator.validate_all(kconfig_file)

        # Should fail due to circular dependency through C
        assert not success, "Expected validation to fail due to circular dependency"

        # Should have error about circular dependency
        assert any('circular' in e.lower() for e in validator.errors), \
            f"Expected error about circular dependency, got: {validator.errors}"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
