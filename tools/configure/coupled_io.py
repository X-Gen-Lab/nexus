"""Dispatch coupled personality review through the selected SoC contract."""
from tools.configure.providers.common import fail


def validate_soc(soc, provider):
    """Unreviewed providers cannot inherit a coupled physical personality."""
    review = getattr(provider, "COUPLED_IO", None)
    if review is not None:
        review.validate_soc(soc)
        return
    if any(controller["kind"] == "can_fd" or
           "pwm_adc" in controller.get("interfaces", {})
           for controller in soc["controllers"].values()):
        fail("Coupled I/O facts require an explicitly reviewed SoC domain")
