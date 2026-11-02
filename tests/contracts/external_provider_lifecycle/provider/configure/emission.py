"""Reference-only immutable Native GPIO faces and explicit C lifecycle calls."""

def implementation(item):
    del item
    return "gpio"


def operations(item):
    del item
    return "nx_native_gpio_ops"


def includes(result):
    del result
    return ['#include "lifecycle_adapter.h"']


def instance_declarations(item, result):
    del item, result
    return []


def state_declarations(result):
    del result
    return []


def shared_irqs(result):
    del result
    return []


def stop_system(result, *, system_phase=None):
    del result
    return [f"    {system_phase};"] if system_phase is not None else []


def start_system(result):
    del result
    return []


def start_resources(result, *, cleanup="nx_platform_stop()"):
    del result, cleanup
    return []
