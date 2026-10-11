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

`ownership.py` validates `.github/maintainer-roles.json`: ten seats, unique named
accounts, review paths, distinct backup roles and security/manufacturing/field
interfaces. An empty `members` array is a valid template and a **pending** report.
The strict command exits nonzero until all seats have real assignments. No real
account names are currently provided; repository membership and branch rules
remain unverified.

```sh
python tools/maintenance/ownership.py
python tools/maintenance/ownership.py --require-assigned
```

When the responsible team supplies actual GitHub usernames or team slugs, fill
each role's `members` and mark `assignment_status` as `assigned`. Produce a file
for review, verify every account has the required repository access, then commit
the reviewed file as `.github/CODEOWNERS` using the normal checks:

```sh
python tools/maintenance/ownership.py --require-assigned \
  --output build/quality/CODEOWNERS.proposed
```

The generator refuses to write the active `.github/CODEOWNERS` directly. Broad
patterns precede specific overrides because GitHub uses the last matching line.
Each pattern names the accountable role and its backup reviewers. CODEOWNERS
itself does not enforce review: repository administrators must configure and
verify branch rules and required CI checks separately. Keep that state pending
until confirmed by GitHub, never infer it from this local template.

`doctor`, `check` and Tool Contracts CI validate the generated capability text
and report the assignment status. Pending real members do not block software
development; use the strict check as the exit condition for team handover.

The installed source SDK intentionally omits repository docs, named assignments
and the main firmware host contract suite. `doctor` first verifies its file map,
package exports,
dependency and source-import identities, then reports tools and this narrower
scope. A manifest's presence alone never skips verification. `check` requires a
development checkout and explicitly rejects the SDK-only scope.
