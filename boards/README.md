# Reviewed Board facts

A Board package records PCB/source identity, the exact SoC, oscillators,
reviewed controller bindings, safe initial levels, reserved resources,
provenance and explicitly unknown electrical facts. It reuses the selected
[SoC provider](../soc/README.md); it does not copy drivers or vendor SDKs.

```text
boards/<board>/
  board.json                  reviewed facts and permitted bindings
  README.md                   source, connector and qualification limits
```

Private PCB packages belong in external product repositories and are selected
through `board_package`. Maintained public packages are selected through `board`.
Keep the family identifier independent of the manufacturer's physical directory.
Board declarations cannot change a chip's memory geometry or enable an
unmaintained clock profile, DMA route or controller mode.

Start with an exact part and a small reviewed route set, then follow the
[Board porting guide](../docs/guides/board-porting.md). A syntactically valid JSON
file does not establish fitted components, connector continuity, power supply
or physical timing. Uninspected PCB revisions and jumpers remain unknown;
software fixtures do not qualify a real PCB. MCU physical HIL remains
`not_executed`.
