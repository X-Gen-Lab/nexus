# Wait ABI cost model

`scripts/ci/os_wait_cost.py` compares the production four-pointer
`nx_wait_port_t` with an experimental two-pointer face and one shared readonly
operations table. The experimental layout is confined to this C fixture.

Run from the current source checkout with the maintained ARM GNU toolchain:

```sh
python scripts/ci/os_wait_cost.py --output build/os-wait-cost-fresh
```

The output directory must be new. Existing evidence and symlink destinations
are rejected; no files are deleted. The tool records exact source and compiler
identities, actual argv/return codes/raw logs, 48 linked ELF files, size/nm
outputs, decoded dispatch instructions and ABI attributes. Missing tools,
missing symbols, writable shared tables, incomplete execution and changed
source inputs fail the comparison.

The reviewed models cover M0+, M4 and M33 without DSP, `Os`/`O2`, one/two/four/
eight retained faces, and both layouts. CPU flags come from the production
resolver, including enum ABI. These are software models with fixed allocation
addresses, not new SoC, Board or startup integrations. They do not run on hardware.

The current default keeps direct callbacks: one owned notification does not need
an additional table load in arm/wait/wake. A shared layout saves eight bytes of
RAM per retained ARM face but adds a twelve-byte readonly table and extra
dispatch loads. Multiple initialized faces can also reduce aggregate Flash;
compare actual linked rows for the selected profile and object count.

RAM and Flash measurements are allocation costs, and instruction counts describe
decoded bodies only. They do not establish cycle latency, WCET or interrupt
jitter. Application stack/register spills, temporary by-value ports, LTO and
their own resource layout need measurements from the actual product image.
