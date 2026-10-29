---
id: docs-tutorials-source-sdk
type: tutorial
audience: consumer
source_language: en
---

(docs-tutorials-source-sdk)=
# Build an independent Source SDK consumer

Start a real external C11 application from a reviewed template, then build and
run its Native model. This path needs Linux, Python 3.11+, CMake 3.31+, Ninja,
a C compiler and Git. The product owns its assembly and application sources;
Nexus owns reusable mechanisms. No MCU is flashed by this tutorial.

## Create and run an application

Obtain a verified Source SDK and the external
[nexus-examples](https://github.com/X-Gen-Lab/nexus-examples) checkout. That
repository is private and requires its own GitHub access; public Nexus or SDK
access does not grant template access. The independent SDK verification below
does not require that private repository. Set
`NEXUS_SDK_PREFIX` to the installed SDK prefix, `NEXUS_EXAMPLES_DIR` to that
checkout and `NEXUS_EXAMPLES_REVISION` to the exact 40-character examples commit
recorded with the chosen platform delivery. These are separate identities: the
template's expected platform revision does not prove which SDK was used.
Run these commands from `$NEXUS_SDK_PREFIX/share/nexus/src`; choose a new product
directory outside both input checkouts:

```sh
python tools/dev/dev.py create --templates "$NEXUS_EXAMPLES_DIR" \
    --revision "$NEXUS_EXAMPLES_REVISION" --template native \
    --output /tmp/my-nexus-product
cmake -S /tmp/my-nexus-product -B /tmp/my-nexus-product-build -G Ninja \
    -DNexus_DIR="$NEXUS_SDK_PREFIX/lib/cmake/Nexus"
cmake --build /tmp/my-nexus-product-build --parallel 2
ctest --test-dir /tmp/my-nexus-product-build \
    --output-on-failure --no-tests=error
```

Creation reads committed template blobs and their SHA-256 manifest without
network access. It rejects a different Git HEAD, changed template bytes, a
symlink or an existing destination. It resolves the copied assembly before
success; failure removes only its newly created destination. Inspect
`.nexus-template.json`: `expected_platform` is the template's lock expectation;
`validation` records the resolver and input bytes actually checked.

The generated product contains `CMakeLists.txt`, `assembly.toml`, `main.c` and
`README.md`. A successful Native test prints
`Nexus starter: transfer settled; platform stopped` and exits zero. Native UART
is a software model, not a host serial port. The example uses strong factory
references, provides request storage, observes settlement and stops the platform.
A timeout is an error, not permission to release outstanding storage.

For a development checkout instead of an installed SDK, replace `Nexus_DIR`
with `-DNEXUS_SOURCE=/absolute/path/to/nexus`. This exercises that checkout and
does not qualify an SDK. Select `qiming`, `sky` or `liangshan` for the maintained
MCU template. SDK consumers add
`-DCMAKE_TOOLCHAIN_FILE="$NEXUS_SDK_PREFIX/share/nexus/src/cmake/toolchains/arm-gcc.cmake"`;
checkout consumers instead select
`-DCMAKE_TOOLCHAIN_FILE=/absolute/path/to/nexus/cmake/toolchains/arm-gcc.cmake`.
Use the locked ARM compiler. An MCU link is software evidence; board bring-up
and electrical verification remain separate.

If creation rejects configuration, read the reported file, key and constraint.
Check the selected SDK/interface and fixed template revision first. A template
correction requires a newly reviewed commit, not edits to pinned template bytes.
After successful creation, edit the product's authored assembly and configure
again; use the [configuration and ownership guide](../guides/developer-workflow.md)
to inspect reviewed bindings. Do not repair generated files or suppress a rejected
selection. Keep product sources; remove only your own build output after
retaining needed logs.

## Maintainer: verify the complete SDK consumer

To verify the examples repository itself, use its maintained
`scripts/local_ci.py` and bilingual `docs/local-ci.md` at the fixed examples
revision. The default scope runs the complete mapped local workflow from a new
output directory; `--scope native`, `--scope arm` and preset selection remain
partial scopes. Its explicit `--nexus-source` must match the examples lock.
An optional `--installed-sdk` must match the same pin. This workflow does not
turn a blocked hosted CI run into a passed one, nor establish physical HIL.

The remaining sections are platform maintenance tasks. First obtain the
[fixed source and recursive dependencies](../guides/source-checkout.md).
Run from that checkout, not an arbitrary default branch. The maintained contract
below checks independent C and C++ consumption, relocation and package identity.

Work from the repository root with the pinned recursive dependencies initialized.
Keep output outside the source checkout. This exact test calls the production
packager and maintained consumer template, moves the prefix into a path containing
spaces, configures independent projects, builds both languages and runs the Native
executables:

```sh
python tests/contracts/test_source_sdk.py     SourceSDKTests.test_relocated_c_and_cpp_consumer_build_and_run_without_original_source
```

Success is a nonempty unittest execution ending in `OK`, with exit code zero.
The test's temporary directory is removed automatically. Failure preserves its
actual command diagnostic; do not count a missing compiler as a passed consumer.
The fixture deliberately packages a development snapshot and explicitly opts in;
this validates consumption, not a publishable software candidate.

The complete consumer is maintained in the production verifier rather than copied
into this page. These literal inputs are the same templates exercised by the test:

```{literalinclude} ../../cmake/package/verify_consumers.py
:language: python
:start-at: def prepare_consumer(
:end-before: def prepare_modern_consumer(
:caption: Maintained Native/F407/F470 consumer preparation; template code is not a shell command.
```

## Maintainer: prepare a publishable prefix

From an initialized, clean, committed checkout, choose a new owned output path:

```sh
python cmake/package/package_source_sdk.py --source . --output /tmp/nexus-source-sdk
python cmake/package/package_source_sdk.py     --verify /tmp/nexus-source-sdk/share/nexus/src
```

Move the entire prefix, including its identity and licenses, before consumption.
The [Source SDK contract](../design/source-compatibility.md) defines
version and interface epoch checks. Missing, extra, modified or symlinked payload
files reject configuration. A package made with `--development-fixture` remains
nonpublishable and requires `NEXUS_ALLOW_SOURCE_SDK_FIXTURE=ON` in its consumer.
Never use that opt-in to bypass release qualification.

The SDK retains the complete recursive CMSIS-Core `Include` tree and its
Apache-2.0 `LICENSE` from the locked official v6.4.0 source. It does not include
the entire CMSIS repository or automatically enable CMSIS-RTOS, CMSIS-Driver,
DSP or NN. These private implementation headers are distinct from the public
`Nexus::Core` mechanisms. Device/startup and HAL/LL remain separately locked;
older bundled GigaDevice Core files retain their original bytes and notices
without becoming the selected common Core. Verify the complete moved prefix,
not a manually copied subset of headers. This dependency update does not expand
supported chips or replace current-source compiler and consumer evidence.

The formal installed-prefix verifier requires the locked ARM compiler on `PATH`
in addition to host tools. It builds Native C/C++ and the maintained MCU consumers:

```sh
python cmake/package/verify_consumers.py     --prefix /tmp/nexus-source-sdk --output /tmp/nexus-source-sdk-verification
```

Check `verification.json`, every recorded exit code, Native runtime results, MCU
ELF/BIN/map and resource reports. MCU linking does not execute hardware. The
verifier's output is owned; an unrelated existing directory rejects replacement.
The SDK prefix and output must be canonical paths without symlink aliases, and
neither may contain the other. A rejected path leaves SDK bytes and previous
reports untouched; read the failure diagnostic on stderr. An admitted owned
output is cleared before the next attempt.
Remove only outputs you created after retaining required evidence.

## Choose a product assembly

Products keep `assembly.toml`, `main`, workers and private Board data in an external
repository. They consume `Nexus::Platform` through `nexus_add_firmware()`; the
helper does not generate an application or start a scheduler. Use the
[assembly reference](../reference/configuration/assembly.md) to select reviewed
bindings and the [factory explanation](../explanation/factory.md) to obtain faces.
Public product examples live in
[nexus-examples](https://github.com/X-Gen-Lab/nexus-examples).

![Source SDK preparation, relocation and independent consumer verification](../assets/diagrams/source-sdk-consumer.svg)

FIG-10 shows the current software consumption flow. Runtime and MCU link evidence
remain separate from [physical qualification](../reference/platforms/qualification.md).

## Maintainer: recover delivery without moving an existing tag

Release maintainers normally use the same commit for SDK source and the
delivery workflow. If only delivery tooling needs repair after the source has
been tagged, keep that tag fixed. The `recover_tagged_source` input defaults to
`false`; only an explicit recovery with `publish=true` allows the tagged source
and the current `main` workflow builder to differ. Same-source runs do not
require `builder_ci_run_id`.

Before recovery, open this repository's **Settings → Secrets and variables →
Actions → New repository secret** and create `NEXUS_RELEASE_TOKEN`. Use an
approved credential restricted to this repository with Actions read, Contents
write and Workflows write permissions. See the
[publishing contract](../../tools/release/README.md#publishing-credentials)
for token types, expiry and approval. Enter the value only in GitHub's secret
field; never send it in chat. Connecting GitHub to the assistant does not create
this Actions secret.

Recovery rejects a missing secret before SDK work. This check proves only that
the secret is configured, not that its permissions, approval or expiry are
valid. Only the final recovery publication step uses it; SDK preparation,
downloads and attestation retain the built-in workflow credentials. An ordinary
publication from one source and builder commit always uses the built-in token.

From the repository root, use an authenticated GitHub CLI with Actions write
permission. Set `NEXUS_RELEASE_SOURCE` to the tagged 40-character source
commit. Set `NEXUS_CI_RUN_ID` to its successful full CI run on `main` and
`NEXUS_BUILDER_CI_RUN_ID` to the successful full CI run for the current `main`
builder. Both runs must have been created within the last 24 hours; source CI
cannot qualify a different builder. After each delivery-tool repair, select the
new builder's CI run, keeping the source commit and tag unchanged. Then dispatch:

```sh
gh workflow run release.yml --ref main \
    -f expected_commit="$NEXUS_RELEASE_SOURCE" \
    -f ci_run_id="$NEXUS_CI_RUN_ID" \
    -f builder_ci_run_id="$NEXUS_BUILDER_CI_RUN_ID" \
    -f publish=true -f recover_tagged_source=true
```

The workflow derives the builder from its actual `main` workflow and checkout.
Recovery requires identical source and builder version-policy bytes, the
existing tag pointing to the source, and the live `main` reference pointing to
the builder. The SDK, documentation and source CI retain the tagged source
identity; the delivery receipt records the builder separately. Policy
mismatches reject admission, and both references are checked before and after
asset uploads.

Keep every original evidence file when repairing packaging limits. Do not move
the tag, omit evidence or substitute a different source CI run to obtain
success. A dispatch alone does not establish publication: inspect the completed
workflow, release manifest and assets, then independently download and verify
the SDK. Recovery does not establish physical HIL or production qualification.
