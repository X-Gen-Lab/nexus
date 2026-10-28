# Maintained capability and review checks

`capabilities.py` uses SoC/routes/Board facts and the production resolver/emitter
to check one deterministic generated block in `docs/delivery/capabilities.md`.
It does not accept a second support configuration. Source ops evidence means an
exact initialized production definition exists; it does not mean that hardware
or the new HEAD has been qualified. Initialize the pinned SDK submodules before
running the resolver-backed check.

`capabilities.py --assembly <assembly.toml>` prints the selected source and
support projection. Its top-level `silicon_admission` belongs to the system-clock
owner; the same field under each controller belongs only to that controller.
These fields project existing reviewed policy, not observed chip identity.
Unreviewed owners report `null`; endpoints and companion controllers do not
inherit another owner's policy. H743 high-clock and SPI DMA reviews and G474 ADC
review/workaround records keep their exact profile, revision and scope fields.
They neither admit an unknown revision nor establish physical qualification.

Add `--candidate /path/to/sealed/candidate.json` to associate an existing
software candidate with this exact assembly. The query runs the existing full
candidate verifier, including referenced payloads, qualifications and linked
artifacts. It does not execute new tests or publish a candidate. Both the
observed current Git source and the referenced tested source are reported;
moving a checkout does not change its commit/tree identity. A dirty checkout,
changed source or different recursive dependency state cannot inherit a pass.

The `evidence.relation` is `exact_source_and_configuration` only when clean
source identity and the complete resolved configuration match. That relation
projects the candidate's existing software scopes into `validation`. A valid
candidate with another source is `historical_source`; a different configuration
is `configuration_mismatch`. Both retain `not_assessed` for the current software
selection. Missing, changed or invalid evidence rejects the query rather than
becoming historical success. Physical HIL remains separately unassessed, even
when the software candidate is valid. No candidate changes the aggregate family
table or qualifies other Boards, controllers, modes or routes.

Without `--candidate`, queries keep their configuration-only behavior and need
no Git or evidence observation. A portable candidate must first be restored with
the existing artifact tools; this command reads the seal and its payloads without
modifying them. Candidate validation may use disposable temporary SDK files.

```sh
python tools/maintenance/capabilities.py --check
python tools/maintenance/capabilities.py --write
python tools/maintenance/capabilities.py \
  --assembly tools/configure/assemblies/h743-high-peripherals.toml
python tools/maintenance/capabilities.py \
  --assembly tools/configure/assemblies/qiming-baremetal-uart-dma.toml \
  --candidate /path/to/sealed/candidate.json
python -B -m unittest tools.testing.test_maintenance -v
```

`local_ci.py --report /path/to/report.json` verifies an existing complete
execution of the checked-in `tools/dev/plans/software.json`. Use the Python
interpreter recorded by that execution. The verifier requires the same clean
source before, after and now; exact plan order, arguments and checker bytes;
actual successful command and unique attempt receipts; and unchanged stdout,
stderr and logs. Empty actual output is accepted only with its completed
receipt. Partial, interrupted, skipped, changed or combined historical plans
cannot publish a pass for a new commit.

The default operation is offline and read-only. Explicit `--publish` uses the
existing GitHub transport and configured `GH_TOKEN` to post the fixed status
`nexus/local-software-plan` to the verified commit. It observes an open PR to
this repository's `main` before and after verification, rejecting ordinary
head/base drift. Status creation is bound to that SHA; it is not an atomic PR
merge operation and a later head cannot inherit the old status. A new receipt
retains the request and confirmed response, or an unconfirmed outcome if the
remote operation fails. No credentials are included in the receipt.

```sh
.venv/bin/python tools/maintenance/local_ci.py --report /path/to/report.json
.venv/bin/python tools/maintenance/local_ci.py --report /path/to/report.json \
  --publish --repository X-Gen-Lab/nexus --pull-request "$NEXUS_PR_NUMBER" \
  --output /path/to/new-status-receipt.json
```

This status means only the maintained local software plan passed for its exact
source. It neither represents GitHub Actions jobs nor establishes a sealed
candidate, physical HIL, release publication or branch protection. Required
checks must name this actual producer and be verified separately on the server.

`work_packages.py` derives a review/navigation plan from existing provider,
capability, resolver fixture, SDK-required-source and reviewed lifecycle-test
authorities. Each entry has an exact family/controller/mode identity; these are
mode review entries, not an additional top-level implementation ledger.

Run from the source checkout root, using a fresh output path:

```sh
python -B tools/maintenance/work_packages.py \
  --output build/quality/work-packages.json
```

Success means those source relationships are consistent. The tool does not run
tests, verify an installed SDK or establish support or qualification. It keeps
`software_status` and `hardware_status` as `not_executed`, reports unmapped
properties without guessed aliases, and records the input files it observes.
Those hashes cover report authorities, not complete Git/configuration/dependency
provenance. `source_selected` is a resolver source relationship, while
`reviewed_bounded` identifies a matching existing reviewed property. Neither
means passed execution. `not_catalogued` does not assert that no other test exists.
The output is created exclusively; select a new path rather than overwrite a
prior report. Source authorities cannot be used as output destinations.

`ownership.py` validates `.github/maintainer-roles.json`. The current
`agent_managed` policy assigns maintenance to Codex across every review domain,
including security, manufacturing and field interfaces. The ten seats describe
the original capacity plan; they do not require named people. Empty `members`
arrays are required in this mode. The strict check validates the responsible
agent, complete paths, backup domains and mandatory automated gates.

```sh
python tools/maintenance/ownership.py
python tools/maintenance/ownership.py --require-assigned
```

GitHub CODEOWNERS requires actual GitHub accounts or team slugs. Agent-managed
maintenance does not invent an account for Codex and refuses CODEOWNERS output.
Independent human review, repository access and branch rules remain separately
unverified; an assigned maintenance policy does not establish these conditions.

If named-account maintenance is selected later, remove `maintenance`, fill each
role's `members` and mark `assignment_status` as `assigned`. Verify account
access before installing a reviewed `.github/CODEOWNERS` through normal checks:

```sh
python tools/maintenance/ownership.py --require-assigned \
  --output build/quality/CODEOWNERS.proposed
```

The generator refuses to write the active `.github/CODEOWNERS` directly. Broad
patterns precede specific overrides because GitHub uses the last matching line.
CODEOWNERS itself does not enforce review. Confirm branch rules and required CI
checks through GitHub before reporting them as configured.

`doctor`, `check` and Tool Contracts CI validate the generated capability text
and report the assignment status. `--require-assigned` passes for a complete
agent-managed policy without requesting human accounts.

`job_identity.py` records each published CI artifact's actual checkout, tree,
recursive dependency state, locks, configuration input closure, executable
bytes, version-command output and artifact digests. A successful job rejects
dirty source, missing required dependencies, incomplete outputs, stale inputs
and files or tools that change during observation. Failed or cancelled jobs
retain missing outputs and invalid configuration bytes with their actual state.
Documentation and style jobs explicitly observe uninitialized SDK dependencies;
they cannot report those dependencies as complete. CPU runtime inputs use the
same length-framed assembly/facts hash as the production resolver.

The composite `record-identity` action supplies inputs through environment
variables and constructs an argument list. It never executes input text as a
shell command. Ordinary artifacts retain the identity and raw version logs for
30 days. The manual source SDK workflow also requests GitHub provenance for the
observed manifest and retains the returned bundle. Workflow configuration is
not evidence that signing ran; GitHub must actually execute it successfully.
The job identity explicitly remains `not_a_software_candidate`. Formal offline
rebuild, source SDK consumption and physical qualification keep their separate
admission rules.

```sh
python tools/maintenance/job_identity.py --help
python -B -m unittest tools.testing.test_job_identity -v
```

The installed source SDK intentionally omits repository docs, named assignments
and the main firmware host contract suite. `doctor` first verifies its file map,
package exports,
dependency and source-import identities, then reports tools and this narrower
scope. A manifest's presence alone never skips verification. `check` requires a
development checkout and explicitly rejects the SDK-only scope.

`compatibility.py` checks the single
`dependencies/platform-compatibility.json` policy against actual CMake,
validator/emitter schemas, fact files and executed exact-version package tests.
The production stdlib loader is `tools/configure/metadata.py`. Unknown requested
source factory generations fail closed; current epoch 1 does not promise a
binary ABI. Optional generated-header and installed-source-SDK checks verify
actual bytes. This metadata consistency check is not software qualification.

`dependency_proposals.py` observes actual current pins offline by default.
Explicit `--observe-upstream --upstream-output <fresh-directory>` captures
bounded public GitHub heads/tags and raw request/response identities. Failures
remain `not_observed`. Codex owns weekly proposals and independent review.
Candidate source/archive/license identities and nine mandatory software/legal
gates are retained as pending or referenced/unreviewed; a passed marker never
authorizes an upgrade. The tool never writes locks or merges source changes.
See [source compatibility and maintenance](../../docs/design/source-compatibility.md)
for scope, commands and the complete upgrade process.

`source_catalogue.py` separates official `upstream_url`, approved `fetch_url`
and the selected Gitlink. Upstream observations follow the official URL even
when retrieval uses a reviewed fork. The catalogue explicitly includes nested
FreeRTOS modules; host-only GoogleTest remains outside the production SDK.
Hosting activation and the reviewed FreeRTOS transport commit are separate
from upgrading the official base. The linked contract also defines checkout
migration order, rejection of local changes and offline SDK trust boundaries.
