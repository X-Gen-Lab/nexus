"""Load the real F4 data package for pure policy and projection tests.

No family adapter, compatibility shim or copied author table participates.
Each call reads one fresh, byte-sealed package and freezes the resulting view.
"""
from dataclasses import dataclass
from pathlib import Path
import shutil
from types import MappingProxyType

from tools.configure.configure import _SnapshotReader, resolve_device_facts
from tools.configure.data_provider import DataProvider
from tools.configure.device_view import DeviceView
from tools.configure.packages import Package, load


_ROOT = Path(__file__).resolve().parents[3]


@dataclass(frozen=True)
class F4Fixture:
    """One actual package/provider pair and the bytes used to construct it."""

    part: str
    package: Package
    provider: DataProvider
    device: DeviceView
    snapshots: MappingProxyType


def load_f4_fixture(part, root=_ROOT):
    """Construct one explicit exact-part view, without a cached fallback."""
    reader = _SnapshotReader({})
    package = load(Path(root) / "soc/stm32/stm32f4/package.json", read=reader)
    selected = DataProvider.from_package(package, reader(package.fact("soc")),
        reader(package.fact("bindings")), read=reader)
    for name in package.facts:
        reader(package.fact(name))
    scoped = resolve_device_facts(reader(package.fact("soc")), part, selected,
        catalog=reader(package.fact("device_catalog")))
    return F4Fixture(part, package, scoped.provider, scoped.device,
                     MappingProxyType(dict(reader.snapshots)))


def provider(part, root=_ROOT):
    """Return the same explicit producer API used by resolved assemblies."""
    return load_f4_fixture(part, root).provider


def copy_package(root, source_root=_ROOT):
    """Copy the actual data package for isolated, no-vendor-I/O fact tests."""
    relative = Path("soc/stm32/stm32f4")
    destination = Path(root) / relative
    shutil.copytree(Path(source_root) / relative, destination)
    return destination
