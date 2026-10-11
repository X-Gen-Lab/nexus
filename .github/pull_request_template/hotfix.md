## Failure and fix

Concrete trigger, affected source/configuration and observed failure:
Root cause and resulting behavior:
Related issue and responsible reviewer:

## Evidence

Provide the reproduction and meaningful regression actually executed. Include
fresh report paths and affected ARM linkage/resource checks where applicable.
State retained request/hardware responsibility on failure.

## Actual RED and GREEN

Write the regression before fixing new or changed behavior. Include the RED
command, failing case, expected failure reason and original raw log; then include
the GREEN command and original passing log. Re-run the relevant regressions after
refactoring. Historical test migration must be identified without a fabricated
RED run. The gate establishes current execution, not historical development order.

## Delivery impact

Describe affected external consumers, any product persistence migration and the
remaining physical verification. An urgent fix still uses the same commit/CI
checks; software CI does not automatically promote a product release.
