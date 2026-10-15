"""Verify installed source SDK bytes at each public resolver boundary."""
import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ".nexus-source-sdk.json"


def installed(root):
    path = Path(root) / MANIFEST
    return path.exists() or path.is_symlink()


def verify(root):
    """Return a completely verified manifest; never cache a byte proof."""
    if not installed(root):
        return None
    script = ROOT / "cmake/package/package_source_sdk.py"
    specification = importlib.util.spec_from_file_location(
        "nexus_configuration_sdk_verifier", script)
    if specification is None or specification.loader is None:
        raise ValueError("Installed SDK verifier is unavailable")
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module.verify(Path(root))
