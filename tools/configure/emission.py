"""Small shared declaration helpers for already validated configuration IR."""
try:
    from .providers.common import fail
except ImportError:
    from providers.common import fail


def dma_regions(name, result, include_flash=False, *, engine=None):
    """Emit immutable exact silicon DMA domains without runtime allocation."""
    if include_flash and any("dma_engines" in region
                             for region in result["memory"]):
        # RAM bus reachability never implies access to the Flash bus. New
        # heterogeneous engines need reviewed Flash facts before enabling it.
        fail("Engine-specific Flash reachability is not declared")
    regions = []
    for region in result["memory"]:
        if "dma_engines" in region:
            if engine is None or engine not in region["dma_engines"]:
                fail("DMA memory projection requires an exact declared engine")
            reachable = region["dma_engines"][engine]
        else:
            reachable = region["dma"]
        if reachable:
            regions.append(region)
    if not regions and not include_flash:
        return [], 0
    lines = [f"static const nx_dma_memory_region_t s_nx_dma_regions_{name}[] = {{"]
    for region in regions:
        lines.append(f"    {{ (uintptr_t)0x{region['origin']:08x}u, {region['size']}u, NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE }},")
    if include_flash:
        flash = result["flash"]
        lines.append(f"    {{ (uintptr_t)0x{flash['origin']:08x}u, {flash['size']}u, NX_DMA_MEMORY_READ }},")
    lines.append("};")
    return lines, len(regions) + int(include_flash)
