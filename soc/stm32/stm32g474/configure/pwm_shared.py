"""One selected physical TIM3 construction, two existing readonly PWM faces."""
from tools.configure.calls import Address, DirectCall
from tools.configure.providers.common import Construction, fail, integer


MODE = "shared-fixed-pwm"
BASE_FIELDS = ("period_ticks", "tick_hz")
_PADS = {("PA6", "ch1", 2): 1, ("PA7", "ch2", 2): 2}


def selected(item):
    return item["kind"] == "pwm" and item["mode"] == MODE


def channel(route):
    pins = tuple((pin["pin"], pin["function"], pin["af"])
                 for pin in route["pins"])
    if route["controller"] != "TIM3" or len(pins) != 1 or pins[0] not in _PADS:
        fail("Shared PWM requires exact TIM3 CH1 PA6 / CH2 PA7 AF2 routes")
    return _PADS[pins[0]]


def validate_selection(selection, route):
    """The compatibility contract is the implemented fixed timer base."""
    channel(route)
    tick = integer(selection.get("tick_hz"), 1, 170000000, "Shared PWM tick")
    if 170000000 % tick or 170000000 // tick > 65536:
        fail("Shared PWM tick must exactly divide the 170MHz timer clock")
    period = integer(selection.get("period_ticks"), 1, 65535,
                     "Shared PWM period")
    integer(selection.get("duty_ticks"), 0, period, "Shared PWM duty")


def resolve(controllers, bindings, soc, clock, clock_profile):
    """Derive one owner only from the complete validated exact channel pair."""
    members = [item for item in controllers if selected(item)]
    if not members:
        return []
    if (clock_profile != "hse24-pll170" or
            soc["controllers"]["TIM3"].get("clock_hz") != 170000000 or
            clock["apb1_hz"] != 170000000):
        fail("Shared PWM requires the reviewed 170MHz TIM3 kernel")
    lanes = {}
    base = None
    for item in members:
        validate_selection(item, item)
        lane = channel(item)
        if lane in lanes:
            fail("Shared PWM pair repeats a physical channel")
        values = (item["tick_hz"], item["period_ticks"])
        if base is not None and base != values:
            fail("Shared PWM members require compatible tick and period base")
        base = values
        binding = bindings[item["binding"]]
        pin = item["pins"][0]["pin"]
        bit = 1 << int(pin[2:])
        if (type(binding.get("mask")) is not int or binding["mask"] != bit or
                type(binding.get("initial")) is not int or
                binding["initial"] not in (0, bit)):
            fail("Shared PWM requires explicit exact Board mask/inactive latch")
        lanes[lane] = {"id": item["id"], "channel": lane, "pin": pin,
                       "duty_ticks": item["duty_ticks"],
                       "inactive": binding["initial"]}
    if set(lanes) != {1, 2} or len(members) != 2:
        fail("Shared PWM requires both members of its exact two-channel pair")
    return [{"provider": "stm32g474", "controller": "TIM3", "mode": MODE,
             "kernel_hz": soc["controllers"]["TIM3"]["clock_hz"],
             "tick_hz": base[0], "period_ticks": base[1],
             "members": [lanes[lane] for lane in (1, 2)]}]


def resources(owners):
    """Compatibility is selected by the actual provider, never an author flag."""
    records, sharing = [], {}
    for owner in owners:
        base = "controller:" + owner["controller"]
        sharing[base + "/base"] = BASE_FIELDS
        source = "stm32g474/" + MODE
        for member in owner["members"]:
            records.append({"key": base + "/base", "owner": member["id"],
                            "mode": MODE, "source": source,
                            "policy": "compatible",
                            "attributes": {key: owner[key] for key in BASE_FIELDS}})
            records.append({"key": base + "/channel/" + str(member["channel"]),
                            "owner": member["id"], "mode": MODE,
                            "source": source})
        # A physical quiet-vector/clock/reset lease belongs to both faces.
        # The existing owner membership syntax is also used for shared EXTI.
        names = ";".join(member["id"] for member in owner["members"])
        for key in ("clock:TIM3", "reset:TIM3", "irq:TIM3"):
            records.append({"key": key, "owner": names, "mode": MODE,
                            "source": source})
    return records, sharing


def state_symbol(owner):
    return "s_nx_physical_pwm_shared_" + owner.controller.lower()


def constructor(owner):
    """The physical owner is one existing Construction lifecycle record."""
    if owner.provider != "stm32g474" or owner.controller != "TIM3" or owner.mode != MODE:
        fail("Shared PWM construction requires its exact typed physical owner")
    state = state_symbol(owner)
    name = "pwm_shared_" + owner.controller.lower()
    config = "s_nx_physical_config_" + name
    first, second = owner.members
    definitions = [
        f"static const nx_stm32g474_pwm_shared_config_t {config} = {{",
        f"    .tick_hz = {owner.tick_hz}u, .period_ticks = {owner.period_ticks}u,",
        f"    .duty_ticks = {{ {first.duty_ticks}u, {second.duty_ticks}u }},",
        f"    .inactive = {first.inactive | second.inactive}u, .poll_limit = 1000000u",
        "};",
    ]
    return Construction(
        state_type="nx_stm32g474_pwm_shared_state_t",
        definitions=definitions, initialize=DirectCall("nx_stm32g474_pwm_shared_initialize",
                              (Address(state), Address(config))),
        stop=(DirectCall("nx_stm32g474_pwm_shared_close", (Address(state),)),),
        retain_on_failed_init=True)


def face(item, owner):
    """The old public ABI points directly at an immutable physical lane ref."""
    member, = [member for member in owner.members if member.id == item.id]
    name = item.id.replace("-", "_")
    reference = "s_nx_channel_" + name
    return [
        f"static const nx_stm32g474_pwm_shared_channel_t {reference} = {{",
        f"    .owner = &{state_symbol(owner)}, .channel = {member.channel}u",
        "};",
        f"static const nx_pwm_port_t s_nx_face_{name} = {{",
        f"    &nx_stm32g474_pwm_shared_ops, (void*)&{reference}",
        "};",
        f"const nx_pwm_port_t* const nx_binding_{name} = &s_nx_face_{name};",
    ]
