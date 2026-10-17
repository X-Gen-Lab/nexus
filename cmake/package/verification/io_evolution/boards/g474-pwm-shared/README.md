# G474 shared TIM3 software reference

This reference declares STM32G474RET6 PA6/TIM3_CH1 and PA7/TIM3_CH2 AF2 for
the explicit `shared-fixed-pwm` selection. Both public PWM faces use one physical
TIM3 owner and one fixed time base. The second channel joins the current phase;
this reference does not promise coherent simultaneous start or a full first pulse.

The Board declares a24MHz HSE input, inactive-low PA6 and inactive-high PA7.
These are software test inputs. No physical PCB, connector, output load,
electrical safety, clock measurement or HIL result is established.

The sole normal input is
[`g474-pwm-shared.toml`](../../g474-pwm-shared.toml). Static route selection,
actual provider behavior, linked-image cost and physical qualification have
separate evidence boundaries.
