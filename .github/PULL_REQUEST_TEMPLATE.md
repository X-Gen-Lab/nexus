## Problem and resulting behavior

Describe the concrete trigger, observed failure and behavior after this change.

## Requirement, ownership and risk

- Requirement/backlog IDs: <!-- NEX-REQ-nnn; BAS/HAL/OS/BSP/HIL/CFG/APP/SEC/REL/PROD/etc. -->
- Primary role and backup: <!-- See .github/maintainer-roles.json; assign actual reviewers in the PR. -->
- Risk: <!-- routine / public contract / persistence / startup-update / product safety -->
- ADR, recovery or format migration: <!-- required when those contracts change; otherwise explain why not applicable. -->

Architectural refactoring may remove defective legacy designs. Update callers and tests together; persisted product data still needs an explicit recovery/migration policy.

## Verified target and evidence

| Evidence | Exact identity or executed result |
|---|---|
| Source and dependency revisions | |
| Product / board revision / SoC | |
| OSAL / compiler / effective configuration digest | |
| Build and linked image/map hashes | |
| Host contracts, executed count and failures/skips | |
| Actual board HIL, deadline/resource measurements | |
| Fault injection, persistence and security checks | |
| Same-artifact inventory / SBOM / signature | |

Mark unavailable or unexecuted evidence explicitly. Adapter models are host tests. A workflow definition, test count or uploaded CodeQL database alone is not product acceptance.

## Validation commands and limits

```sh
# Provide the commands actually executed and the relevant report paths.
```

Describe known limits, unsupported combinations and remaining hardware/production inputs.

## Review and delivery

- [ ] Changed contracts document context, deadline, ownership, cancellation and failure state.
- [ ] Critical failures have behavioral regression coverage.
- [ ] Support claims and examples match implementation and executed evidence.
- [ ] Release/manufacturing artifacts retain source/config/dependency/image identity and contain no credentials.
- [ ] Reviewers cover the responsible role and product risk; formal promotion uses the configured evidence gates.
