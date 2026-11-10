"""Selected USB-specific ROM profiles, separate from generic packet limits."""

from tools.configure import usb_otg


def _profile(item, result, *, fallback=False):
    """Emit readonly selected or ULPI full-speed fallback metadata."""
    name = item.id.replace("-", "_")
    children = [child for child in result.devices if child.controller == item.id]
    array = (f"s_nx_usb_fs_fallback_endpoints_{name}" if fallback else
             f"s_nx_usb_profiles_{name}")
    identity = (f"s_nx_usb_fs_fallback_profile_{name}" if fallback else
                f"s_nx_usb_profile_{name}")
    lines = [f'static const nx_usb_endpoint_profile_t {array}[{len(children)}] = {{']
    for child in children:
        endpoint = child.id.replace("-", "_")
        options = child.options
        transfer = {"control": "NX_USB_PROFILE_CONTROL",
                    "bulk": "NX_USB_PROFILE_BULK",
                    "interrupt": "NX_USB_PROFILE_INTERRUPT",
                    "isochronous": "NX_USB_PROFILE_ISOCHRONOUS"}[options.transfer]
        maximum = (min(options.max_packet,
                       1023 if options.transfer == "isochronous" else 64)
                   if fallback else options.max_packet)
        lines.append(f'    {{ &s_nx_device_face_{endpoint}, {maximum}U, '
                     f'{options.number}U, {str(options.direction == "in").lower()}, '
                     f'{transfer} }},')
    hs = usb_otg.device_mode(item.mode) != "device-fs"
    ulpi = usb_otg.device_mode(item.mode) == "device-hs-ulpi"
    frequency = "60000000U" if ulpi else "NX_USB_FS_PHY_CLOCK_HZ"
    fifo = "1024U" if hs else "NX_USB_FS_FIFO_WORDS"
    numbers = "6U" if hs else "NX_USB_FS_ENDPOINT_NUMBERS"
    speed = ("NX_USB_PROFILE_HIGH_SPEED" if ulpi and not fallback else
             "NX_USB_PROFILE_FULL_SPEED")
    phy = "NX_USB_PROFILE_EXTERNAL_ULPI" if ulpi else "NX_USB_PROFILE_INTERNAL_FS"
    engine = ("NX_USB_PROFILE_AHB_DMA" if item.options.engine == "dma"
              else "NX_USB_PROFILE_PIO")
    setup_ops = ("nx_stm32_usb_otg_setup_decision_ops"
                 if usb_otg.is_managed(item.mode) else "nx_usb_fs_setup_decision_ops")
    lines += ['};', f'static const nx_usb_device_profile_t {identity} = {{',
              f'    &s_nx_face_{name}, {array}, {len(children)}U,',
              f'    {frequency}, {fifo}, {item.options.rx_fifo_words}U, {numbers},',
              f'    NX_USB_PROFILE_DEVICE, {speed}, {phy}, {engine},',
              f'    {{ &{setup_ops}, &s_nx_port_{name} }}', '};']
    return lines


def emit(result):
    """Bind validated endpoints and their two explicit ULPI speed profiles."""
    controllers = [item for item in result.controllers if item.kind == "usb_device"]
    if not result.factory.capabilities or not controllers:
        return [], []
    header = ['#include "nexus/io/usb_profile.h"',
              r'/** \brief Pure selected USB facts, never hardware qualification. */',
              'const nx_usb_device_profile_t* nx_factory_usb_profile(',
              '    nx_usb_device_id_t id);',
              r'/** \brief Pure speed-specific facts; choose after ENUMERATED. */',
              'const nx_usb_device_profile_t* nx_factory_usb_profile_for_speed(',
              '    nx_usb_device_id_t id, nx_usb_profile_speed_t speed);']
    source = []
    for item in controllers:
        source.extend(_profile(item, result))
        if usb_otg.device_mode(item.mode) == "device-hs-ulpi":
            source.extend(_profile(item, result, fallback=True))
    source += [r'/** \brief Pure typed lookup; no start, allocation or MMIO. */',
               'const nx_usb_device_profile_t* nx_factory_usb_profile(',
               '    nx_usb_device_id_t id) {', '    switch (id) {']
    for item in controllers:
        name = item.id.replace("-", "_")
        source += [f'        case NX_USB_DEVICE_ID_{name.upper()}:',
                   f'            return &s_nx_usb_profile_{name};']
    source += ['        default:', '            return NULL;', '    }', '}',
               r'/** \brief No runtime discovery or hardware speed change. */',
               'const nx_usb_device_profile_t* nx_factory_usb_profile_for_speed(',
               '    nx_usb_device_id_t id, nx_usb_profile_speed_t speed) {',
               '    switch (id) {']
    for item in controllers:
        name = item.id.replace("-", "_")
        source += [f'        case NX_USB_DEVICE_ID_{name.upper()}:']
        if usb_otg.device_mode(item.mode) == "device-hs-ulpi":
            source += ['            if (speed == NX_USB_PROFILE_FULL_SPEED)',
                       f'                return &s_nx_usb_fs_fallback_profile_{name};',
                       '            if (speed == NX_USB_PROFILE_HIGH_SPEED)',
                       f'                return &s_nx_usb_profile_{name};',
                       '            return NULL;']
        else:
            source += ['            return speed == NX_USB_PROFILE_FULL_SPEED',
                       f'                ? &s_nx_usb_profile_{name} : NULL;']
    source += ['        default:', '            return NULL;', '    }', '}']
    return header, source


def emit_activation(result):
    """Expose selected controller operations independently of ROM metadata."""
    controllers = [item for item in result.controllers
                   if item.kind == "usb_device"]
    header = (['#include "nexus/io/usb_activation.h"'] if controllers else [])
    header += ['/** \\brief Fixed controller activation identity, or NULL.',
              ' * \\note Task/ISR safe; no start, locks or hardware access.',
              ' */',
              'const nx_usb_activation_port_t* nx_factory_usb_activation(',
              '    nx_usb_device_id_t id);']
    source = []
    for item in controllers:
        name = item.id.replace("-", "_")
        activation_ops = ("nx_stm32_usb_otg_activation_ops"
                          if usb_otg.is_managed(item.mode) else "nx_usb_fs_activation_ops")
        source += ['static const nx_usb_activation_port_t '
                   f's_nx_usb_activation_{name} = {{',
                   f'    &{activation_ops}, &s_nx_port_{name}', '};']
    source += ['/** \\brief Pure lookup; no start or hardware access. */',
               'const nx_usb_activation_port_t* nx_factory_usb_activation(',
               '    nx_usb_device_id_t id) {', '    switch (id) {']
    for item in controllers:
        name = item.id.replace("-", "_")
        source += [f'        case NX_USB_DEVICE_ID_{name.upper()}:',
                   f'            return &s_nx_usb_activation_{name};']
    source += ['        default:', '            return NULL;', '    }', '}']
    return header, source
