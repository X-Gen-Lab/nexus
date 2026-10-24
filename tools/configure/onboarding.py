"""Validate a provider's selected configuration through its real static emitter.

This is a developer check, not a build graph or a qualification database. CMake
and the maintained model/station suites remain the compile and runtime gates.
"""

import hashlib
from pathlib import Path
import tempfile

try:
    from . import emission, selected_contracts
    from .providers import descriptor, maintained
    from .providers.common import fail
except ImportError:
    import emission
    import selected_contracts
    from providers import descriptor, maintained
    from providers.common import fail


def check(result, *, expected_family=None, expected_part=None):
    """Emit the exact selection into disposable storage, without compiling."""
    if expected_family is not None and result.soc_family != expected_family:
        fail("Provider check family differs from the resolved exact family",
             code="provider_identity_mismatch", expected=expected_family,
             actual=result.soc_family)
    if expected_part is not None and result["part"] != expected_part:
        fail("Provider check part differs from the resolved exact part",
             code="provider_identity_mismatch", expected=expected_part,
             actual=result["part"])
    provider = maintained(result.soc_family)
    family = descriptor.maintained(result.soc_family)
    # Use the production emitter once. It exercises provider constructors,
    # nominal factory faces, cold start/stop and compiler/linker projections.
    with tempfile.TemporaryDirectory(prefix="nexus-provider-check-") as work:
        output = Path(work)
        emission.emit_bundle(result, output)
        generated = {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                     for path in sorted(output.iterdir()) if path.is_file()}
    controllers = []
    for item in result.controllers:
        contract = selected_contracts.describe(item)
        controllers.append({
            "id": item.id, "controller": item.controller,
            "kind": item.kind, "mode": item.mode,
            "public_class": contract.public_class,
            "implementation": provider.mode_implementation(
                item.kind, item.mode),
        })
    return {
        "schema_version": 1, "kind": "provider_configuration_check",
        "checks": ["resolved_typed_ir", "static_bundle_emitted"],
        "family": result.soc_family, "part": result["part"],
        "board": result["board"], "backend": result["backend"],
        "configuration_sha256": result["configuration_sha256"],
        "controllers": controllers,
        "configuration_modules": list(family.configuration_modules),
        "fact_inputs": [dict(item) for item in result["inputs"]],
        "generated_files": generated,
        "physical_verification": ("not_applicable" if provider.MODEL else
                                  "not_established_by_configuration"),
        "scope": "Selected configuration and static generation only; "
                 "compile/link, model tests and applicable station evidence "
                 "must pass their existing maintained gates.",
    }
