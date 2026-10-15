"""Small shared declaration helpers for already validated configuration IR."""


def dma_regions(name, result, include_flash=False):
    """Emit immutable exact silicon DMA domains without runtime allocation."""
    regions = [region for region in result["memory"] if region["dma"]]
    lines = [f"static const nx_dma_memory_region_t s_nx_dma_regions_{name}[] = {{"]
    for region in regions:
        lines.append(f"    {{ (uintptr_t)0x{region['origin']:08x}u, {region['size']}u, NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE }},")
    if include_flash:
        flash = result["flash"]
        lines.append(f"    {{ (uintptr_t)0x{flash['origin']:08x}u, {flash['size']}u, NX_DMA_MEMORY_READ }},")
    lines.append("};")
    return lines, len(regions) + int(include_flash)
