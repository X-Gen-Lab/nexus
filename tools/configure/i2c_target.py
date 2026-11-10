"""Finite I2C target choices; protocol register/product policy stays external."""
from tools.configure.providers.common import fail, integer


FIELDS = frozenset(("own_address", "address_bits", "second_address",
                    "max_hz", "irq_latency_us", "pec", "alert"))


def validate(selection, route):
    """Match explicit hardware facilities to one reviewed static route."""
    if not (FIELDS | {"irq_priority", "calls_os"}).issubset(selection):
        fail("I2C target requires explicit address, speed, latency, PEC, alert and IRQ fields")
    for name in ("pec", "alert"):
        if type(selection[name]) is not bool:
            fail("I2C target " + name + " must be boolean")
    width = integer(selection["address_bits"], 7, 10, "I2C target address_bits")
    if width not in (7, 10):
        fail("I2C target address_bits must be 7 or 10")
    integer(selection["own_address"], 0 if width == 10 else 8,
            1023 if width == 10 else 119, "I2C target own_address")
    second = integer(selection["second_address"], 0, 119,
                     "I2C target second_address")
    if second and (second < 8 or second == selection["own_address"]):
        fail("I2C target second_address requires a distinct ordinary 7-bit address")
    smbus = selection["mode"] == "smbus-device"
    if width == 10 and (second or smbus):
        fail("I2C target 10-bit addressing excludes dual address and SMBus")
    maximum = integer(selection["max_hz"], 1, 400000, "I2C target max_hz")
    latency = integer(selection["irq_latency_us"], 1, 0xffffffff,
                      "I2C target irq_latency_us")
    if selection["calls_os"] is not False:
        fail("I2C target IRQ has no OS callback")
    if ((not smbus and (selection["pec"] or selection["alert"])) or
            (smbus and maximum > 100000)):
        fail("SMBus PEC/alert require smbus-device at no more than 100 kHz")
    if selection["pec"] and latency * maximum >= 6000000:
        fail("SMBus PEC irq_latency_us must be below six external SCL periods")
    if len(route["pins"]) != (3 if selection["alert"] else 2):
        fail("I2C target alert requires its exact optional SMBA route")
    if selection["alert"] and route["pins"][2]["function"] != "smba":
        fail("I2C target requires reviewed SMBA alternate function")


def editor_schema(schema):
    """Editor bounds mirror the same immutable input fields."""
    schema["required"] = sorted(set(schema["required"]) | FIELDS | {"irq"})
    properties = schema["properties"]
    for name, maximum in (("own_address", 1023), ("second_address", 119),
                          ("max_hz", 400000), ("irq_latency_us", 0xffffffff)):
        properties[name] = {"type": "integer", "minimum": 0 if "address" in name else 1,
                            "maximum": maximum}
    properties["address_bits"] = {"type": "integer", "enum": [7, 10]}
    for name in ("pec", "alert"):
        properties[name] = {"type": "boolean"}
