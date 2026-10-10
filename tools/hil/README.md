# HIL preparation and admission

`prepare.py` turns a generated `resolved.json` and one maintained fixture into
an unbound work plan. It validates the exact part and smoke-console pins and
retains selected controller modes. It records every physical case as
`not_executed`; it cannot establish station readiness or hardware qualification.

```sh
python tools/hil/prepare.py \
    --fixture tools/hil/fixtures/gd32f470_liangshan.json \
    --resolved build/gd32-liangshan-baremetal/generated/resolved.json \
    --report build/gd32-liangshan-baremetal/hil-preparation.json
```

Copy `fixtures/station.template.json` into station-owned storage when equipment
is connected. Observe the part, PCB revision, chip UID, probe serial, stable
serial path, wiring, ground and supply. Content-hash the actual flashing tool
and probe configuration files. Review numeric measurement budgets against the
intended workload; null fixture budgets require that review.

`admit.py` checks the observed station, exact linked ELF, configuration and
recomputed resource budget before equipment operation. It also validates raw
physical records against that admission. Software models, a copied report,
an empty execution, a changed ELF and a stale record cannot qualify hardware.
The four smoke tests qualify only their recorded scope. Pending DMA, IRQ,
stream, ADC, cancellation, timing and power-loss cases require their own
physical measurements before those capabilities can be qualified.

The platform supplies tooling and generic measurement cases. Products own
electrical limits, fixture wiring, erase regions, real workloads and release
decisions. Unknown PCB routes remain unknown until reviewed. Neither command
operates probes, powers boards, flashes firmware or opens serial devices.
