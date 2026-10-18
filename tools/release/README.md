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

Both dispatch modes require the requested source commit to equal the trusted
main workflow commit before executing checkout-owned scripts. A publishing
dispatch also requires a successful full CI run on `main` for that commit
created within the last 24 hours, and an existing matching version tag. Every
required CI job must have actually succeeded. The current `main` and tag are
checked again before publication completes. A qualification dispatch executes
the maintained build and contract workflows without publishing a release. Both
dispatch modes use the trusted main workflow.

The SDK source checkout and the workflow builder commit remain separately
recorded identities; normal main dispatches require their commits to coincide.
Attestation verification binds the actual builder commit and ref; SDK identity
verification binds the requested source and executed qualification. A builder
signature does not establish a different SDK source commit by itself.

## Raw evidence and publication

The immutable consumer plan in `transport.py` is shared by the installed SDK
producer and the standalone versioned transport. Its current exact selection
has 31 independent consumer roots and 34 executable images: three C/C++ base
roots, ten MCU/backend roots and eighteen selected-feature roots. Native time
pair/empty executables are host observations; MCU images are linked software
evidence. These counts do not establish physical execution.

New transport manifests use schema `2` and policy `nexus-sdk-consumers-v2`.
Changing the reviewed plan also requires reviewing its policy identity.
Packing and verification require every exact plan root and nonempty image;
missing, additional or same-count substituted Git roots fail. Report/manifest
claims cannot authorize another selection. Each consumer's `.git` is only an
unborn `git init` fixture isolating the SDK from its parent's source identity.
History, remotes, an index, active hooks and foreign Git metadata remain refused.
The SDK requires the complete shared plan module even after a rehashed omission.

Archives retain all original raw bytes, file modes and empty directories, verify
the complete inventory and consume the gzip stream through its CRC/size trailer
before crossing the artifact identity boundary. The outer artifact contains an
inert archive, manifest and original consumer report. Unsafe paths, links,
duplicate entries, trailing compressed content and changed bytes fail. Transport
does not rewrite a failed report or establish that a recorded command executed;
actual source, job and builder identities retain that separate responsibility.

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
