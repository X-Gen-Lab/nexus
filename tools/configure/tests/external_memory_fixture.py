"""Authoritative package data for direct external-memory policy unit tests."""
import json
from pathlib import Path
from tools.configure.policies.external_bus import ExternalBusPolicy


def policy():
    root = Path(__file__).resolve().parents[3]
    path = root / 'soc/stm32/stm32f4/bindings/external_bus_policy.json'
    return ExternalBusPolicy.from_validated(
        json.loads(path.read_text())['domains']['external_bus'])


def construct(item, result):
    """Use the selected provider instance rather than a family module import."""
    from tools.configure.providers import for_selection
    return for_selection(result).constructor(item, result, result.board_bindings,
                                             result.routes, result.devices)


def route_pins(identity):
    root = Path(__file__).resolve().parents[3]
    document = json.loads((root / 'soc/stm32/stm32f4/routes.json').read_text())
    return tuple(next(row for row in document['routes'] if row['id'] == identity)['pins'])
