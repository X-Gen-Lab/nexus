"""Reviewed G474 coupled personalities, with no product or runtime policy."""
from collections.abc import Mapping

try:
    from .providers.common import fail, integer
except ImportError:
    from providers.common import fail, integer


MODES = {"pwm_adc": ("finite-regular-dma",),
         "can_fd": ("iso-fd-loopback", "iso-fd-normal")}
PWM_PINS = (("PA8", "ch1", 6), ("PA9", "ch2", 6), ("PA10", "ch3", 6),
            ("PA0", "analog", 0), ("PA1", "comp1-inp", 0),
            ("PA4", "comp1-inm", 0))
CAN_PINS = (("PA11", "rx", 9), ("PA12", "tx", 9))
PWM_RESOURCES = ("controller:ADC1", "controller:ADC2", "controller:ADC345",
                 "controller:DMAMUX1_Channel0", "irq:DMA1_Channel1", "irq:ADC1_2",
                 "clock:ADC12", "clock:DMA1", "clock:DMAMUX1")
CAN_RESOURCES = ("clock:FDCAN", "message-ram:FDCAN1")


def validate_soc(soc, provider):
    """Tie new interfaces to exact physical IP, not invented controller aliases."""
    for name, controller in soc["controllers"].items():
        if controller["kind"] == "can_fd":
            if (provider.PREFIX != "stm32g474" or name != "FDCAN1" or
                    controller["modes"] != list(MODES["can_fd"]) or
                    controller["irq"] != ["FDCAN1_IT0_IRQn", "FDCAN1_IT1_IRQn"] or
                    controller.get("irq_numbers") != [21, 22] or
                    controller.get("clock_hz") != 24000000 or
                    controller.get("message_ram_words") != 212 or
                    controller["pins"] != ["PA11", "PA12"] or
                    tuple(controller.get("resources", ())) != CAN_RESOURCES):
                fail("CAN FD facts differ from the reviewed G474 physical domain")
        if "pwm_adc" in controller.get("interfaces", {}):
            if (provider.PREFIX != "stm32g474" or name != "TIM1" or
                    controller["kind"] != "pwm_group" or
                    controller["interfaces"]["pwm_adc"] != list(MODES["pwm_adc"]) or
                    controller.get("clock_hz") != 170000000 or
                    not {pin for pin, _, _ in PWM_PINS}.issubset(controller["pins"])):
                fail("PWM/ADC personality requires the single reviewed TIM1 owner")


def validate_selection(selection, route, prefix):
    """Reject every unimplemented mode before publishing an assembly."""
    kind = route["kind"]
    pins = tuple((pin["pin"], pin["function"], pin["af"]) for pin in route["pins"])
    expected = PWM_PINS if kind == "pwm_adc" else CAN_PINS
    controller = "TIM1" if kind == "pwm_adc" else "FDCAN1"
    if (prefix != "stm32g474" or kind not in MODES or
            selection["mode"] not in MODES[kind] or route["controller"] != controller or
            pins != expected):
        fail("Coupled I/O requires the exact reviewed G474 mode and silicon route")
    if (type(selection.get("exclusive")) is not bool or
            not selection["exclusive"] or
            type(selection.get("irq_priority")) is not int or
            selection["irq_priority"] != 5):
        fail("Coupled I/O requires one exclusive owner and IRQ priority5")
    if kind == "pwm_adc":
        for name, value in (("tick_hz", 1000000), ("period_ticks", 100),
                            ("trigger_ticks", 50), ("deadtime_ticks", 170)):
            if type(selection.get(name)) is not int or selection[name] != value:
                fail("PWM/ADC finite window has an unreviewed fixed " + name)
        for name, values in (("channels", (1,)), ("sample_times", (0,))):
            actual = selection.get(name)
            if (not isinstance(actual, (list, tuple)) or tuple(actual) != values or
                    any(type(value) is not int for value in actual)):
                fail("PWM/ADC finite window requires exact ADC1 channel/sample facts")
        integer(selection.get("reference_mv"), 1620, 3600, "PWM/ADC reference")
        if tuple(route.get("resources", ())) != PWM_RESOURCES:
            fail("PWM/ADC route has incomplete coupled resource ownership")
    else:
        for name, value in (("bitrate", 500000), ("data_bitrate", 2000000)):
            if type(selection.get(name)) is not int or selection[name] != value:
                fail("CAN FD has an unreviewed fixed " + name)
        integer(selection.get("event_capacity"), 1, 4096, "CAN FD event capacity")
        integer(selection.get("drain_timeout_us"), 1, context="CAN FD drain bound")
        integer(selection.get("poll_limit"), 1, context="CAN FD poll bound")
        filters = selection.get("filters")
        if not isinstance(filters, (list, tuple)) or len(filters) > 36:
            fail("CAN FD mask filters exceed the physical plan")
        counts = {"standard": 0, "extended": 0}
        for record in filters:
            if (not isinstance(record, Mapping) or
                    set(record) != {"id", "mask", "format", "fifo"} or
                    type(record["format"]) is not str or record["format"] not in counts):
                fail("CAN FD requires exact standard/extended mask filter fields")
            maximum = 0x7ff if record["format"] == "standard" else 0x1fffffff
            integer(record["id"], 0, maximum, "CAN FD filter ID")
            integer(record["mask"], 0, maximum, "CAN FD filter mask")
            integer(record["fifo"], 0, 1, "CAN FD filter FIFO")
            counts[record["format"]] += 1
        if counts["standard"] > 28 or counts["extended"] > 8:
            fail("CAN FD standard/extended filters exceed physical message RAM")
