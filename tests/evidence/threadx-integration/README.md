# ThreadX integration follow-up

This record adds strict declarative backend policy, a locked production CMake
source graph and independent review fixes. It preserves the original IO34
[development record](../threadx/README.md); earlier source snapshots and raw
results are not rewritten to match later implementation bytes.

`review/` retains actual kernel RED failures for five known storage admission
boundaries and four callback context boundaries, corrected fixture build failures
and GREEN raw execution. The callback fixture runs official kernel initialization
in isolated child processes; only Arch facts and product fail-stop are wrapped.
The two canonical context controls prove that valid startup still runs.
`configuration/` retains the strict ThreadX/FreeRTOS policy and compatibility
metadata tool runs, including failed runner setup attempts rather than silently
replacing them. Backend name metadata never authorizes a CPU/Board combination.

Reports qualify dirty-source development software only. Three-board ARM links,
installed SDK, clean candidate, physical MCU/HIL and physical host clock-step
experiments each require separate actual evidence. Nothing in a test count or a
port label establishes a physical result.
