# Maintained capability and review checks

`capabilities.py` uses SoC/routes/Board facts and the production resolver/emitter
to check one deterministic generated block in `docs/delivery/capabilities.md`.
It does not accept a second support configuration. Source ops evidence means an
exact initialized production definition exists; it does not mean that hardware
or the new HEAD has been qualified. Initialize the pinned SDK submodules before
running the resolver-backed check.

```sh
python tools/maintenance/capabilities.py --check
python tools/maintenance/capabilities.py --write
python -B -m unittest tools.testing.test_maintenance -v
```

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
