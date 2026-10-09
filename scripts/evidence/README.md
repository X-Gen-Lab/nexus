# Build inventory and release evidence

All tools use Python 3.11+ standard library. Crypto verification uses the maintained
OpenSSL CLI and only a public key. Model tests and schema validation are not physical HIL,
manufacturing qualification, SLSA certification or license approval.

```sh
python -m unittest discover -s scripts/evidence -p 'test_*.py' -v
python scripts/evidence/check_traceability.py --report build/evidence/traceability.json
python scripts/evidence/build_inventory.py create --manifest /ci/build-input.json \
  --output /ci/inventory.json --sbom /ci/sbom.cdx.json
python scripts/evidence/build_inventory.py verify --inventory /ci/inventory.json
python scripts/evidence/release_gate.py --level candidate --inventory /ci/inventory.json \
  --report /ci/candidate-gate.json
```

`build-input.json` is explicit, with schema version 1 and these fields:

```json
{
  "schema_version": 1,
  "source_root": "/ci/nexus",
  "product": {"id": "industrial-reference", "board_profile": "native-contracts", "board_revision": "not-applicable", "osal": "native"},
  "configuration": {"effective": "/ci/build/generated/effective.config", "header": "/ci/build/generated/nexus_config.h", "cmake": "/ci/build/generated/config.cmake"},
  "toolchain": {"path": "/usr/bin/x86_64-linux-gnu-gcc-14", "version_args": ["--version"]},
  "artifacts": [{"role": "elf", "path": "/ci/build/bin/blinky"}],
  "link_map": "/ci/build/bin/blinky.map",
  "components": [{"name": "nexus-hal", "version": "SOURCE_COMMIT", "license": "MIT", "license_file": "/ci/nexus/LICENSE", "source_root": "/ci/nexus", "archive": "/ci/build/lib/libhal.a"}],
  "tests": {"junit": "/ci/build/ctest-results.xml", "source_commit": "SOURCE_COMMIT", "config_sha256": "EFFECTIVE_CONFIG_SHA256", "artifact_sha256": "ELF_SHA256"}
}
```

Replace uppercase placeholders with actual full identities. Include every linked archive
and its real source repository/license. An `image` artifact role is required for physical
HIL and enterprise promotion. Test bindings must be emitted by a trusted runner that
executed the built contracts; copying passing XML does not establish test provenance.
Empty, failed, skipped or unbound JUnit is rejected. The GNU/LLVM ELF linker map must
reference archive members; a `LOAD lib.a` declaration alone is insufficient. Other map
formats need an explicit parser before claiming coverage.

Inventory contains source commit/tree and tracked-content digest, dirty state, exact
configuration/header/CMake hashes, compiler binary/version, artifact identities,
actual archive member evidence, component source/license digests, executed test count
and a CycloneDX 1.5 JSON SBOM digest. The SBOM scope is **map-linked static archives**.
Undeclared archives are listed; dynamic/system/toolchain runtime license and vulnerability
review remains explicit. It is not a complete firmware SBOM until those inputs are reviewed.
The generator records dirty builds for analysis; either promotion level rejects them.

Component `source_kind` defaults to `git`, requiring `source_root` to be a real Git
repository root. A reviewed SDK copied into the Nexus repository must explicitly
use `"source_kind": "vendor-source-import"`; a folder name never changes the type.
For example, GD32 SDK components use `source_root` ending in
`vendors/gigadevice/gd32f4xx`, version `3.3.3`, and license expression
`BSD-3-Clause AND Apache-2.0 AND LicenseRef-Arm-Cortex-M-2012`. `license_file` must
point to one of that import's tracked `LICENSES` notices. All source-lock license
terms must appear exactly once in the expression. Supported expressions are a
single identifier or an explicit `AND` conjunction; other license semantics need
reviewed schema support. This records licensing evidence and does not approve it.

Typed imports are verified against `source.lock.json` and the owning Nexus HEAD:
every imported source, origin README and license notice must be a regular tracked
blob with unchanged bytes; extra/untracked auxiliary SDK files are rejected.
Inventory and CycloneDX record the upstream download URL/archive digests, lock
digest, notice digests and owning repository commit. They never invent a vendor
Git commit. Vendor version must match the lock. Promotion rechecks the same source
type and content; changing source and lock together without committing a reviewed
import also fails. These additions preserve the explicitly partial SBOM scope.

The verifier rereads every file and current source/dependency state. Changed artifact,
config, map, SBOM, test XML or dependency blocks promotion. Public evidence paths currently
refer to local regular files; relocation needs a controlled path-rebase operation and
digest revalidation before a new signature, not an implicit path rewrite.

For enterprise promotion, review a product policy with schema 1 and fields `product_id`,
`board_profile`, `board_revision`, `trusted_public_key_sha256`, `required_tests`, `budgets`,
`required_reviews`, `runtime_components`. Test/review lists and hardware budgets are
nonempty. Runtime components have `{name, version, license, evidence: {path, sha256, size}}`;
unresolved archive basenames must be covered. Reviews have schema 1,
`inventory_sha256` and `reviews: [{id, status: "pass", owner_role, evidence: {...}}]`.
Security/OTA, power-cut recovery, license disposition and product release authorization
belong in that policy, with actual report identities and accountable reviewers.

The public-key fingerprint is a reviewed trust anchor supplied separately from a
downloaded evidence pack. Prepare the exact public payload for an external signer:

```sh
python scripts/evidence/release_gate.py --inventory /ci/inventory.json \
  --policy /policy/release.json --hil /lab/hil.json --reviews /ci/reviews.json \
  --assemble /ci/release-evidence.json
# The protected signing service signs release-evidence.json with a maintained tool.
# No signing key or enrollment credential is supplied to these scripts.
python scripts/evidence/release_gate.py --level enterprise --inventory /ci/inventory.json \
  --policy /policy/release.json --hil /lab/hil.json --reviews /ci/reviews.json \
  --signed-evidence /ci/release-evidence.json --public-key /policy/signing-public.pem \
  --signature /ci/release-evidence.sig --report /ci/enterprise-gate.json
```

The detached RSA/ECDSA SHA-256 signature is verified by OpenSSL, binds inventory, policy,
physical HIL and reviews, and therefore the same artifact/config/dependency identities.
Missing verifier/signature, a changed trusted public key, model HIL, wrong board/readback,
missing measurements/reviews and changed signed evidence all block enterprise promotion.
The scripts do not publish a GitHub release or rebuild artifacts.

`resource_checks.py size` records actual linked ELF section measurements and maps with
configuration hashes. It explicitly reports product limits and hardware timing pending.
`resource_checks.py memory` enumerates CTest, runs build/bin ELF contracts under Valgrind,
retains finished memcheck XML and rejects tool/contract errors. Non-ELF tool tests are
listed as excluded; zero linked contracts fail. Lost allocations/invalid accesses fail;
still-reachable retained objects are counted. Neither task proves MCU deadline behavior.
