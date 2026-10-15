Independent immutable HTTP restore of remote qualification evidence

Source commit: 58297516d707fdee117bd27b5df6af09d72edd2e
Input asset commit: c90efc92259ddd7da28c65406d5a5e4268840b8e
Input prefix: docs-release-0.1.0-58297516d707-qualification

The verifier downloaded the exact committed asset registry and all 12 selected
files through public commit-specific raw URLs without credentials. It checked
all bytes against the pinned registry, then ran the downloaded stdlib reader.
The independent archive read checked all 58 qualification members and both
51-member examples archives without extraction. Actual reader stdout and all
observed file/member hashes are preserved in restore.json and member-observations.json.

The earlier local invocation supplied a relative output directory and repeated
that path under subprocess cwd after all 12 downloads had verified. Its original
failure receipt is retained as local-invocation-failure.json. The utility now
resolves output paths before use; a fresh directory run passed. No remote
qualification failure or altered evidence bytes were inferred from that local
invocation error. This directory retains the exact successful utility.

An independent read-only review of utility SHA-256
fef21daf156337dc614c43011bf511e610658a346eb8040014d2218215188ea9
confirmed immutable URL and registry binding, verification before reader
execution, safe destination/member paths, complete archive closure and scope.

This is byte restoration of existing remote observations, not a new CI run.
Those observations establish main CI 29/29, Security 4/4, Pages publication and
public HTTP verification, and examples PR/main 11/11. Code-scanning alert APIs
returned 403; no zero-alert inventory is claimed. SDK attempt 38684283007 failed
without attestation or Release. This proof establishes no successful SDK public
Release, firmware candidate qualification, or physical-board/HIL execution.
