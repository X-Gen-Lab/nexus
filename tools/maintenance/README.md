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

The installed source SDK intentionally omits repository docs, named assignments
and the main firmware host contract suite. `doctor` first verifies its file map,
package exports,
dependency and source-import identities, then reports tools and this narrower
scope. A manifest's presence alone never skips verification. `check` requires a
development checkout and explicitly rejects the SDK-only scope.
