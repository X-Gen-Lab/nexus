"""Selected USB-specific ROM profiles, separate from generic packet limits."""


def emit(result):
    """Bind validated typed endpoints to their actual generated static faces."""
    controllers = [item for item in result.controllers
                   if item.kind == "usb_device"]
    if not result.factory.capabilities or not controllers:
        return [], []
    header = ['#include "nexus/io/usb_profile.h"',
              '/** \\brief Pure selected USB facts, never hardware qualification. */',
              'const nx_usb_device_profile_t* nx_factory_usb_profile(',
              '    nx_usb_device_id_t id);']
    source = []
    for item in controllers:
        name = item.id.replace("-", "_")
        children = [child for child in result.devices
                    if child.controller == item.id]
        source.append('static const nx_usb_endpoint_profile_t '
                      f's_nx_usb_profiles_{name}[{len(children)}] = {{')
        for child in children:
            endpoint = child.id.replace("-", "_")
            options = child.options
            transfer = {
                "control": "NX_USB_PROFILE_CONTROL",
                "bulk": "NX_USB_PROFILE_BULK",
                "interrupt": "NX_USB_PROFILE_INTERRUPT",
            }[options.transfer]
            source.append(f'    {{ &s_nx_device_face_{endpoint}, '
                          f'{options.max_packet}U, {options.number}U, '
                          f'{str(options.direction == "in").lower()}, {transfer} }},')
        source += ['};',
            f'static const nx_usb_device_profile_t s_nx_usb_profile_{name} = {{',
            f'    &s_nx_face_{name}, s_nx_usb_profiles_{name}, {len(children)}U,',
            '    NX_USB_FS_PHY_CLOCK_HZ, NX_USB_FS_FIFO_WORDS,',
            f'    {item.options.rx_fifo_words}U, NX_USB_FS_ENDPOINT_NUMBERS,',
            '    NX_USB_PROFILE_DEVICE, NX_USB_PROFILE_FULL_SPEED,',
            '    NX_USB_PROFILE_INTERNAL_FS, NX_USB_PROFILE_PIO,',
            f'    {{ &nx_usb_fs_setup_decision_ops, &s_nx_port_{name} }}', '};']
    source += ['/** \\brief Pure typed lookup; no start, allocation or MMIO. */',
               'const nx_usb_device_profile_t* nx_factory_usb_profile(',
               '    nx_usb_device_id_t id) {', '    switch (id) {']
    for item in controllers:
        name = item.id.replace("-", "_")
        source += [f'        case NX_USB_DEVICE_ID_{name.upper()}:',
                   f'            return &s_nx_usb_profile_{name};']
    source += ['        default:', '            return NULL;', '    }', '}']
    return header, source
