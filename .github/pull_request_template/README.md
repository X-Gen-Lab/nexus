# Maintained PR templates

Use the default template for platform changes, `platform.md` for SoC/Board/provider
work, `simple.md` for small reversible changes, or `hotfix.md` for a concrete
regression. Titles use Conventional Commits and descriptions lead with the
problem and resulting behavior. Examples use `feat(io)`, `fix(os)`, `build(config)`
or `docs(delivery)`.

Review contracts and evidence in [current delivery](../../docs/delivery/README.md)
and use [coding standards](../../docs/sphinx/development/coding_standards.rst).
Local commit hooks and CI share format/comment checks. Native/model execution,
actual ARM linkage and physical HIL remain separate.

Actual reviewers are assigned in the PR; the ten-person role file is not a
substitute for named reviewers or configured branch protection. Product policy,
private PCB data and application workers stay outside this repository.
