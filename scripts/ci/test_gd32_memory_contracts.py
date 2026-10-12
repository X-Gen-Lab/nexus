"""Execute the complete lightweight memory contract in the maintained CI scope."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]


def load_tests(_loader, _tests, _pattern):
    """Load the original host-only module without inheriting test filters."""
    directory = ROOT / "tests/contracts"
    loader = unittest.TestLoader()
    tests = loader.discover(str(directory), pattern="test_gd32_memory_matrix.py",
                            top_level_dir=str(directory))
    if loader.errors or not tests.countTestCases():
        raise ValueError("Complete GD32 memory contract discovery required")
    return tests
