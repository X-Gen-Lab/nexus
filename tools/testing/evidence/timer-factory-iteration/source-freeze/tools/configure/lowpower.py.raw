"""Explicit shallow-sleep port policy; SoC packages own hardware support."""
try:
    from .providers.common import fail
except ImportError:
    from providers.common import fail


PORTS = ("custom", "timebase")


def resolve_port(options):
    """Validate only port policy; selection never creates hardware effects."""
    port = options.get("lowpower_port", "custom")
    if not isinstance(port, str) or port not in PORTS:
        fail("Unsupported lowpower port")
    if port == "timebase":
        if options.get("tickless") is not True:
            fail("Timebase lowpower port requires tickless")
        if options.get("tick_source") != "systick":
            fail("Timebase lowpower port requires explicit SysTick")
        if options.get("memory_protection") is not False:
            fail("Timebase lowpower port requires a privileged kernel")
        if options.get("security_model") != "single":
            fail("Timebase lowpower port requires single-world security")
    return port
