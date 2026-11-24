# Public FOC composition consumer

This consumer executes a synthetic Axis decision, transfers its finite current
reference to Drive, converts calibrated phase codes, plans phase-shunt PWM and
feeds the final quantized voltage back to Motor. It uses public C11 headers and
rejects visible vendor implementation headers.

`native.toml` executes on the explicit Native model. `f407.toml` links the same
consumer for STM32F407VET6 and checks its image resource budget. Neither path
starts a PowerStage session or qualifies a physical motor, current sensor,
sampling aperture, interrupt latency or stack high-water mark.

The canonical Source SDK plan owns both consumers. Source checkouts and relocated
SDKs use the same target graph; no private provider headers or product policy are
copied into the public consumer.
