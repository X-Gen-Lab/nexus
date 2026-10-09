# Platform development and evidence tools

`dev/dev.py` is the thin configure/build/test entry. `configure/` resolves one
SoC/Board/assembly input to an atomic bundle. `measurement/` checks actual ARM
ELF physical regions, selected configuration and budgets, and compares minimum
images across compiler profiles. `hil/` validates station admission and raw
physical evidence without operating unavailable equipment. `evidence/` binds
clean source, exact files, executed logs, SDK, environment and candidate bytes.

```sh
python tools/dev/dev.py configure --preset native-debug
python tools/dev/dev.py build --preset native-debug --parallel 4
python tools/dev/dev.py test --preset native-debug
```

Source SDK preparation requires a clean checkout with initialized pinned
submodules. The installed prefix is verified, including all files and exported
CMake helpers. A nonpublishable development fixture cannot qualify a candidate.
Formal reproduction uses that complete source SDK as its read-only source input:

```sh
python cmake/package/package_source_sdk.py --source . --output /your/source-sdk
python cmake/package/verify_consumers.py --prefix /your/source-sdk --output /your/sdk-checks
python tools/evidence/reproduce.py --prepare-spec --spec /your/repro-spec.json \
  --source . --assembly tools/configure/assemblies/liangshan-baremetal-empty.json \
  --source-sdk /your/source-sdk/share/nexus/src/.nexus-source-sdk.json \
  --environment /your/observed-environment.json --toolchain /your/arm-toolchain
python tools/evidence/reproduce.py --spec /your/repro-spec.json --report /your/repro-report.json
```

Prepare the OCI input before formal execution with `tools/dev/Dockerfile` and
`evidence/container.py seal`, then verify the observed image/OCI archive. The
recipe uses a pinned base and observed dependencies; it does not promise that
mutable preparation repositories recreate the same image. Firmware runs offline
with read-only SDK/toolchain, separate outputs and no credentials mounted.

Candidate sealing requires all executed software qualification scopes and
recursively retains actual raw logs and referenced artifacts. Evidence reports
belong outside the source tree to avoid self-referential source identity. Hardware
measurements, product promotion and signatures stay explicitly separate.
