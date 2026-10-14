---
id: docs-tutorials-source-sdk
type: tutorial
audience: consumer
source_language: en
---

(docs-tutorials-source-sdk)=
# Build an independent Source SDK consumer

This tutorial verifies that an independent C and C++ project can consume a relocated
Nexus Source SDK. It runs on a Linux host with Python 3.11+, CMake 3.31+, Ninja,
a C/C++ compiler and Git. It does not flash or execute an MCU.

## Run the maintained complete consumer

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

## Prepare a publishable prefix

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

The formal installed-prefix verifier requires the locked ARM compiler on `PATH`
in addition to host tools. It builds Native C/C++ and the maintained MCU consumers:

```sh
python cmake/package/verify_consumers.py     --prefix /tmp/nexus-source-sdk --output /tmp/nexus-source-sdk-verification
```

Check `verification.json`, every recorded exit code, Native runtime results, MCU
ELF/BIN/map and resource reports. MCU linking does not execute hardware. The
verifier's output is owned; an unrelated existing directory rejects replacement.
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
