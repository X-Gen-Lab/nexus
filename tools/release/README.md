# Source SDK release maintenance

`delivery.py` stages verified SDK bytes, assembles the complete release asset
set and publishes a software preview. `transport.py` preserves complete raw
consumer output and records the distinct source and workflow builder
identities. The dispatch entry is `.github/workflows/release.yml` on `main`.

## Version and qualification inputs

`dependencies/platform-compatibility.json` is the sole authored version policy.
The existing strict metadata reader supplies the version used by the release
tag, SDK archive root, evidence archive, documentation checks and release
title. Preparing a later version requires a reviewed policy change; these tools
never bump the version, move a tag or replace an existing release
automatically.

By default, both dispatch modes require the requested source commit to equal
the trusted main workflow commit before executing source-owned scripts. A
publishing dispatch requires an explicit successful full CI run on `main` for
that source, created within the last 24 hours, and an existing matching version
tag. Every required CI job must have actually succeeded. A qualification dispatch
executes the maintained build and contract workflows without publishing a release.

Only `publish=true` with explicit `recover_tagged_source=true` admits a separate
immutable tagged source. The guard first checks out the actual workflow builder,
requires its clean HEAD and workflow SHA to agree, checks live main/tag identities,
and compares the tagged compatibility policy as pure Git blob bytes with the
trusted builder policy. It executes no tagged-source script before admission.
Recovery also requires an explicit `builder_ci_run_id` for the builder's own
fresh successful full main CI; `ci_run_id` continues to identify the SDK source CI.
The source SDK job still observes and rebuilds that exact tagged source. The
publisher uses trusted builder tools, retains the source's complete CI evidence
and both builder CI API records, and records their run ID/attempt separately.
Publication rechecks both fresh full CI runs, unchanged attempts, live main at
the builder and the unchanged tag at the SDK source before and after upload.

The SDK source checkout and the workflow builder commit remain separately
recorded identities; normal main dispatches require their commits to coincide.
Attestation verification binds the actual builder commit and ref; SDK identity
verification binds the requested source and executed qualification. A builder
signature does not establish a different SDK source commit by itself.

Release inventories retain a 100,000-file limit per ordinary input. Only the
aggregate of complete source-CI artifacts permits 200,000 files: the observed
29-artifact set contains 103,918 files. Empty and over-limit inputs report their
path, count and limit separately. No artifact is removed to fit the budget;
the evidence archive still verifies every original member's bytes.

## Raw evidence and publication

The immutable consumer plan in `transport.py` is shared by the installed SDK
producer and the standalone versioned transport. Its current exact selection
requires 50 independent consumer roots and 53 executable images under
policy-v11. This is a required plan; actual installed-consumer execution must be
assessed from its source-bound producer report and matching transport manifest.
Plan counts do not establish software qualification or physical acceptance.

New transport manifests use schema `2` and policy `nexus-sdk-consumers-v11`.
Changing the reviewed plan also requires reviewing its policy identity.
Packing and verification require every exact plan root and nonempty image;
missing, additional or same-count substituted Git roots fail. Report/manifest
claims cannot authorize another selection. Each consumer's `.git` is only an
unborn `git init` fixture isolating the SDK from its parent's source identity.
History, remotes, an index, active hooks and foreign Git metadata remain refused.
The SDK requires the complete shared plan module even after a rehashed omission.

Historical policy-v8 required 48 roots and 51 images. It added exactly one
selected MCU consumer relative to policy-v7:
`u575 lptim1 lpbam`, using the canonical
[assembly](../configure/assemblies/u575-lptim1-lpbam-stop2.toml) and public C11
[consumer](../../cmake/package/verification/io_evolution/consumers/u575-lptim1-lpbam.c).
It links `Nexus::U575Stop2Lptim1` as a compile-time alternative of the same
concrete Stop2 owner, with its own public request contract for one finite SRAM4
CCR1/RCR=0/ARR tuple. This is not a repeated or general LPBAM program. The
[reference contract](../../docs/design/u575-lptim1-lpbam.md) defines explicit
start/stop, retained partial-failure responsibility and compound final release.
The additional required root and image do not inherit historical v7 execution,
SDK qualification, pin routing or physical HIL results.

Policy-v9 added `u575 mpu gpio`, using the canonical
[assembly](../configure/assemblies/u575-mpu-gpio-write.toml) and public C11
[consumer](../../cmake/package/verification/io_evolution/consumers/u575-mpu-gpio.c)
with `Nexus::U575MpuGpio`. Its required plan had 49 roots and 52 images.
Policy-v10 added `f407 i2s2 slave tx`, using the canonical
[assembly](../configure/assemblies/sky-i2s2-slave-tx.toml) and public C11
[consumer](../../cmake/package/verification/io_evolution/consumers/f407-i2s2-slave-tx.c)
with `Nexus::F407I2S2SlaveTx`, bringing the plan to 50 roots and 53 images.
These additions specify public software consumers; they do not establish
physical MPU isolation, external audio execution or HIL qualification.

Policy-v11 retains 50 roots and 53 images, but changes the existing `h743 high`
consumer to the canonical
[assembly](../configure/assemblies/h743-high-peripherals.toml). Its consumer name
and `h743-high` smoke remain unchanged. The previous verification-directory
copy is not a fallback. A complete policy-v10 transport with the same root and
image counts still cannot satisfy the current exact plan or policy identity.

Archives retain all original raw bytes, file modes and empty directories, verify
the complete inventory and consume the gzip stream through its CRC/size trailer
before crossing the artifact identity boundary. The outer artifact contains an
inert archive, manifest and original consumer report. Unsafe paths, links,
duplicate entries, trailing compressed content and changed bytes fail. Transport
does not rewrite a failed report or establish that a recorded command executed;
actual source, job and builder identities retain that separate responsibility.

Historical schema-2 policy-v2 31-root/34-image evidence keeps its original
source, bytes and hash-pinned helper. The current policy does not accept it as
a fallback or rewrite it as the 50-root/53-image selection. Earlier policy-v3
34-root/37-image and 36-root/39-image plans also retain their exact historical
source identities; neither is the current plan or a fallback. Historical policy-v4
has 28 selected features, 41 roots and 44 images: H743 SPI DMA Cache on, U575 finite
UART hardware-chain TX, U575 LPTIM2 RUN one-shot, and H743 continuous UART RX
with Cache off and Cache on extended its preceding 23-feature plan. Policy-v5
adds explicit H743 UART continuous DMA duplex with Cache off and Cache on;
policy-v4 evidence retains its own source and helper and cannot satisfy this plan.
The preceding policy-v5 plan has 44 roots and 47 images after adding the U575
RTC RUN consumer. Policy-v6 adds two independent consumers using the maintained
U575 baremetal empty assembly: retained time links `Nexus::U575RetainedTime`,
and the bounded Stop2 owner links `Nexus::U575Stop2`. Their public C11 sources
remain separate from the mutually exclusive RTC RUN factory. These are required
software link checks; they do not execute Stop2 or establish physical wake timing.
That historical policy-v6 selection has 46 roots and 49 images.

Historical policy-v7 requires 47 roots and 50 images. It added the optional
finite U575 SRAM4 copy consumer through
`tools/configure/assemblies/u575-lpdma-copy-stop2.toml` and the public C11
`u575-lpdma-copy` smoke, which links `Nexus::U575Stop2Copy`. It uses the same
Stop2 owner for the bounded copy. Its original source, bytes and hash-pinned
helper remain historical; they could not satisfy policy-v8 and cannot satisfy
the current policy. This required link check
does not record an executed full installed consumer matrix or a physical copy
result. The explicit exact current plan, rather than an old count or the policy
label alone, determines admission; v7 results cannot qualify the added LPTIM1
consumer or the complete v8 matrix.

Historical schema-1 thirteen-root archives keep their original bytes, source and
hash-pinned helper for read-only recovery. Current packing and verification do
not accept them as a fallback or migrate them into current qualification. The
earlier seven-root set is likewise not a current selection. A repaired transport
does not qualify a previously failed SDK or offline-build run; new committed
source needs its own complete successful execution.

Local evidence storage is handled by `tools/evidence/store.py`. Its `copy`
operation preserves an independent content-addressed archive, reads every safe
member, verifies the original again, and writes a preservation receipt. Unlike
`adopt`, which adds a hardlink name, a copy survives later writes to the original
inode. Neither operation establishes offsite retention or durable publication:
new receipts explicitly retain `persistence_status: not_established`, including
when a directory happens to be called `durable`. Legacy receipts remain readable
without inheriting a durability claim.

```sh
python tools/evidence/store.py copy --source candidate.tar.gz \
    --store /path/to/independent-storage --report preservation.json
python tools/evidence/store.py verify --package preservation.json \
    --report verification.json
```

After transporting a portable evidence archive, use its original package with
`restore --archive /new/location/archive.tar.gz --destination /new/restore` and
an explicit `--report` to verify an independent restoration. Preserve the package
and raw verification report alongside the copied bytes. A temporary directory,
local hardlink, file checksum or successful local restore is not an external
retention guarantee. Actual durable delivery also needs a retained remote copy,
its byte identity and availability, and an independent recovery exercise before
the original execution storage is retired.

The publisher verifies the attestation, assembles the complete SDK,
documentation and raw qualification assets, creates a draft, uploads and
downloads every asset for a byte comparison, then publishes the verified set as
a prerelease. A failure leaves an explicit failure or draft for inspection.
Cancellation and unsuccessful direct dependencies prevent the publisher from
running; deliberately skipped qualification ancestors do not suppress a
qualified publishing dispatch.

## Historical release branches

`delivery/0.1.0` is the frozen builder used for the initial preview. Preserve
its commit, the `v0.1.0` tag, published assets and provenance records. Adopt
reusable fixes through reviewed maintenance PRs into `main`; historical
recovery branch names and failed workflow run IDs are not inputs to later
releases.

`documentation-assets` is a separate append-only evidence branch. Keep its
registered bytes and immutable restoration links, and maintain its own asset
verification hook. Its archives belong outside the platform source history.
`docs-delivery` separately owns the published documentation site.

Physical HIL remains `not_executed` until equipment executes the relevant
tests. Host tests, linked images and source provenance do not establish
production qualification, safety certification, signed firmware or an LTS
promise.
