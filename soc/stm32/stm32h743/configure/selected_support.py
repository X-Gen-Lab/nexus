"""Exact H743 selected limits and reviewed silicon expectations."""
from tools.configure.providers.common import fail


def selected_limits(item, shape):
    """Preserve IRQ TX and complete-cache-line DMA bounds independently."""
    if item.kind == "uart" and item.mode == "rx-dma-continuous":
        # TX uses IRQ requests; the distinct RX block loan retains NDTR limits.
        return {"max_transfer_units": "UINT32_MAX"}
    if ((item.kind == "spi" and shape.dma or
         item.kind == "uart" and item.mode == "dma-duplex-continuous") and
            item.options.dma_cache_line_bytes):
        return {"buffer_alignment": 32, "max_transfer_units": "65504"}
    return {}


def selected_format(item, shape):
    """Only the reviewed HSI64 modes derive cache policy from their options."""
    if ((item.kind == "spi" and item.mode == "dma") or
            (item.kind == "uart" and item.mode in
             ("rx-dma-continuous", "dma-duplex-continuous"))):
        line = item.options.dma_cache_line_bytes
        return {"cache_line_bytes": line,
                "dma_policy": ("domain_cache_line" if line else
                               "provider_domain")}
    return {}


def silicon_policy(result, item=None):
    """Project a scoped software expectation; no physical observation occurs."""
    from tools.configure.selected_contracts import SiliconPolicy

    if item is not None:
        if item.kind != "spi" or item.mode != "dma":
            return None
        if (result.backend != "baremetal" or
                result["clock_profile"] not in {
                    "hsi64-fixed-cache-off", "hsi64-fixed-cache-on"}):
            fail("Selected SPI DMA silicon expectation requires its exact profile")
        cache = "on" if result["clock_profile"].endswith("-cache-on") else "off"
        return SiliconPolicy(result["part"], result.soc_family,
                             f"spi4-dma-hsi64-cache{cache}-dev450-v2003-trim64",
                             (0x2003,), "selected_spi_dma_mode")
    plan = result.clock_plan
    if plan is None:
        return None
    return SiliconPolicy(result["part"], result.soc_family, plan.profile,
                         plan.reviewed_revision_ids, "selected_clock_profile")
