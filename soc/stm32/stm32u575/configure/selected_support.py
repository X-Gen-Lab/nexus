"""Exact U575 selected GPDMA descriptor-chain request bound."""


def selected_limits(item, shape):
    """The reviewed finite chain has three 16-bit byte-count descriptors."""
    if item.kind == "uart" and item.mode == "dma-chain-tx":
        return {"max_transfer_units": 196605}
    return {}
