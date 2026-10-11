# Independent optional timer source review — 2026-10-11

This records a read-only review by the Arch/runtime owner; it is not an
independent test execution or physical qualification.

The reviewer found normal input capture read-clear/W0C could discard a new
edge arriving after CCR read. Both actual provider suites reproduced two
failures each: the unread edge disappeared and read-boundary overcapture was
not attached to the published value. Production handling now admits from initial
CCIF, reads CCR (hardware clears the old CCIF), then samples overcapture and
W0C-acknowledges only observed OC. New CCIF remains unread for the next IRQ.
Only the draining discard branch explicitly acknowledges CCIF. The reviewer
reread this final ordering and closed the finding.

Other reviewed contracts were retained ring/config/table alias protection,
failed detach preserving the ring, static release requiring outer publisher
join, and BIF setup → DSB → pending-break observation → MOE → actual readback
and final break observation. LOCK/AOE are excluded and tDTS is independent of
PSC. No further blocking path was identified within serialized privileged
Thread control, same-CPU configurable IRQ and exclusive outer clock/pin/vector
ownership. Failed start retains controller cleanup responsibility until explicit
stop even when the rejected caller ring is no longer borrowed.

Electrical inactive drive/readback belongs to Board/cold assembly. A physical
pulse wholly inside a read/write interval, shared-vector load, edge rate and
maximum ISR latency require actual station measurement and remain not executed.

Reviewed source identities:

- io/include/nexus/io/timer_capture.h: 68af76fbd4961d6fd9fd2a158cbdb4f2e0e04562e1996e5c02c8035a25269816
- io/include/nexus/io/advanced_pwm.h: 2b9bbfbb94b834ed25964f5aa6495abc4e5cbc5f5d5bb48d106e951f37f52410
- io/src/timer_capture.c: 65845a51610421f6fd1072ffe06eb726bbaabe950ca4a8d47bdb16e78cd2a945
- io/src/advanced_pwm.c: 9505b6a8913d23978defb59e18eabc4c0dfa43252104e526205b8528b3cf28c5
- io/private/timer_advanced_controller.h: bd4cc4f7c704b8f3cc8016e2845c8bb546a51b610c0e2c4fc72b5747498bbd77
- io/private/timer_capture_impl.h: 2e3c53f3692bb77c733cf211c97e4b7d767a4bc4b88979eb31b58b4c4d03a066
- io/private/advanced_pwm_impl.h: 940c43b4015f4a4ead9f2184628e16b3888a9424b5053e4e8c4110c74785468e
- tests/google/timer_advanced_test.cpp: 2c3e64306b7914d1f66133a20eb7c2ccee88bd194a9c87762722b9c2e82deee8
- tests/google/timer_advanced_model.h: 341eb2af4ba125bee6beafbe5387f12da413d0c15a5daf75386d8993fcd73f23
- soc/stm32f407/drivers/timer_capture.c: 3cdbaaaae4eed8fd5b3c1f26690000e0cb808bfeac8b5bbf999fe94d08b14da4
- soc/stm32f407/drivers/advanced_pwm.c: d41e0e9312446aaf4391d7c709e398439e4c7686227758741e066329f093744f
- soc/stm32f407/private/stm32f407_timer_advanced.h: 40af97908cbb09db80f8a70a950c334fd881aa0e288e74036ba7007fef8ab3da
- soc/gd32f470/drivers/timer_capture.c: decb30804b049687cd755c11b1e53b3a5a6471e30736f785326b38c793ee12db
- soc/gd32f470/drivers/advanced_pwm.c: e3008f26eef7f868770a21d24532dc43e67dd5ef5109d7ef5f97d76644618a3a
- soc/gd32f470/private/gd32f470_timer_advanced.h: b865ef153518445378d36cc72544fb0a99503c1d59decb76180c88519a18b689
