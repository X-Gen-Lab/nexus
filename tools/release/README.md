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

Strict consumer output includes seven unborn Git fixtures. Transport archives
retain their original bytes, file modes and empty directories, then verify the
complete inventory before crossing the artifact identity boundary. The outer
artifact contains an inert archive, manifest and original consumer report.
Verification rejects unsafe paths, links, extra files and changed bytes.

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
