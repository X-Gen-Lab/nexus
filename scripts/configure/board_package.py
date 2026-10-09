#!/usr/bin/env python3
"""Validate a Board package and compile one consumer-owned Flash layout.

The manifest is hardware/build metadata, not an alternative software config.
Only maintained SoCs and explicit resource routes can be extended. This proves
structural consistency; board wiring and electrical behavior still require HIL.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys


SOCS = {
    'native': {'platform': 'native', 'flash_size': 0, 'ram_size': 0, 'irq_count': 0},
    'stm32f407vg': {'platform': 'stm32', 'flash_size': 0x100000, 'ram_size': 0x20000, 'irq_count': 82},
    'stm32f407zg': {'platform': 'stm32', 'flash_size': 0x100000, 'ram_size': 0x20000, 'irq_count': 82},
    'stm32f407ve': {'platform': 'stm32', 'flash_size': 0x80000, 'ram_size': 0x20000, 'irq_count': 82},
    'gd32f470zg': {'platform': 'gd32f470', 'flash_size': 0x100000, 'ram_size': 0x30000, 'irq_count': 91},
}
# Supported resource routes of the maintained providers. Expanding this table
# requires SDK/board/provider review and actual cross-build coverage.
ROUTES = {
    ('stm32', 'uart', 0): ('USART1', 37, {'tx': ('A', 9, 7), 'rx': ('A', 10, 7)}),
    ('stm32', 'uart', 1): ('USART2', 38, {'tx': ('A', 2, 7), 'rx': ('A', 3, 7)}),
    ('stm32', 'spi', 1): ('SPI1', 35, {'sck': ('A', 5, 5), 'miso': ('A', 6, 5), 'mosi': ('A', 7, 5)}),
    ('gd32f470', 'uart', 0): ('USART0', 37, {'tx': ('A', 9, 7), 'rx': ('A', 10, 7)}),
    ('gd32f470', 'spi', 4): ('SPI4', 85, {'sck': ('F', 7, 5), 'miso': ('F', 8, 5), 'mosi': ('F', 9, 5)}),
}
DMA_ROUTES = {('stm32', 'spi', 1): {'tx': ('DMA2', 3, 3, 59), 'rx': ('DMA2', 0, 3, 56)}}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def number(value, field):
    require(type(value) in (int, str), f'{field}: expected integer or hex string')
    try:
        result = int(value, 0) if isinstance(value, str) else value
    except ValueError as error:
        raise ValueError(f'{field}: invalid integer') from error
    require(0 <= result <= 0xffffffff, f'{field}: outside uint32 range')
    return result


def exact_keys(value, allowed, required, label):
    require(isinstance(value, dict), f'{label}: expected object')
    require(not set(value) - set(allowed), f'{label}: unknown keys {sorted(set(value) - set(allowed))}')
    require(set(required) <= set(value), f'{label}: missing keys {sorted(set(required) - set(value))}')


def config_values(path):
    values = {}
    for line in Path(path).read_text().splitlines():
        match = re.fullmatch(r'(CONFIG_[A-Z0-9_]+)=(.*)', line)
        if match:
            raw = match[2]
            values[match[1]] = raw == 'y' if raw in ('y', 'n') else raw.strip('"')
        else:
            match = re.fullmatch(r'# (CONFIG_[A-Z0-9_]+) is not set', line)
            if match:
                values[match[1]] = False
    return values


def digest_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def checked_input(root, relative):
    require(isinstance(relative, str) and relative, 'inputs: expected nonempty relative path')
    candidate = root / relative
    require(not Path(relative).is_absolute() and '..' not in Path(relative).parts,
            f'input path escapes Board package: {relative}')
    require(not candidate.is_symlink() and candidate.is_file(), f'Board input missing/nonregular: {relative}')
    require(candidate.resolve().is_relative_to(root.resolve()), f'Board input escapes package: {relative}')
    return candidate


def validate_manifest(directory, config):
    root = Path(directory).resolve()
    raw = json.loads((root / 'board.json').read_text())
    exact_keys(raw, ('schema', 'id', 'soc', 'hse_hz', 'interface_target', 'object_targets', 'inputs', 'resources'),
               ('schema', 'id', 'soc', 'hse_hz', 'interface_target', 'object_targets', 'inputs', 'resources'), 'Board')
    require(raw['schema'] == 1, 'Board: unsupported schema')
    require(re.fullmatch(r'[a-z][a-z0-9_-]{1,79}', raw['id']) is not None, 'Board: invalid id')
    require(raw['soc'] in SOCS, 'Board: unsupported/unmaintained SoC')
    soc = SOCS[raw['soc']]
    require(config.get('CONFIG_PLATFORM_NAME') == soc['platform'], 'Board SoC/platform conflict')
    if soc['platform'] == 'stm32':
        require(config.get('CONFIG_' + raw['soc'].upper()) is True, 'Board/config silicon density conflict')
    crystal = number(raw['hse_hz'], 'hse_hz')
    if soc['platform'] != 'native':
        key = 'CONFIG_STM32_HSE_VALUE' if soc['platform'] == 'stm32' else 'CONFIG_GD32_HXTAL_VALUE'
        require(crystal == number(config[key], key), 'Board crystal/effective clock conflict')
    else:
        require(crystal == 0, 'Native Board has no physical crystal')
    targets = [raw['interface_target'], *raw['object_targets']]
    require(isinstance(raw['object_targets'], list) and len(targets) == len(set(targets)), 'Board: duplicate object target')
    for target in targets:
        require(isinstance(target, str) and re.fullmatch(r'[a-zA-Z][a-zA-Z0-9_]*', target), 'Board: invalid CMake target')
    require(isinstance(raw['inputs'], list) and raw['inputs'], 'Board: nonempty inputs required')
    require(len(raw['inputs']) == len(set(raw['inputs'])) and 'CMakeLists.txt' in raw['inputs'], 'Board: inputs must include unique CMakeLists.txt')
    inputs = {name: digest_file(checked_input(root, name)) for name in raw['inputs']}
    require(isinstance(raw['resources'], list), 'Board: resources must be a list')
    pins, irqs, dma, controllers, active = {}, {}, {}, set(), []
    resource_ids = set()
    for resource in raw['resources']:
        exact_keys(resource, ('id', 'kind', 'instance', 'when', 'clock', 'irq', 'priority', 'pins', 'dma'),
                   ('id', 'kind', 'when', 'pins'), 'resource')
        require(isinstance(resource['id'], str) and re.fullmatch(r'[a-z][a-z0-9_-]{0,47}', resource['id']) and resource['id'] not in resource_ids, 'invalid/duplicate resource id')
        resource_ids.add(resource['id'])
        require(resource['when'] in config, f"resource {resource['id']}: unknown Kconfig condition")
        if config[resource['when']] is not True:
            continue
        require(resource['kind'] in ('gpio', 'uart', 'spi'), 'unsupported controller resource')
        key = (soc['platform'], resource['kind'], resource.get('instance'))
        route = None
        if resource['kind'] != 'gpio':
            require(key in ROUTES, f'unsupported/unreviewed peripheral route: {key}')
            require(key not in controllers, 'duplicate controller binding')
            controllers.add(key)
            clock, irq, route = ROUTES[key]
            require(resource.get('clock') == clock and resource.get('irq') == irq, 'controller clock/IRQ mismatch')
            require(type(resource.get('priority')) is int and 0 <= resource['priority'] <= 15, 'invalid IRQ priority')
            if config.get('CONFIG_OSAL_FREERTOS') is True:
                # Maintained FreeRTOSConfig.h fixes the library priority to 5.
                minimum = 5
                require(resource['priority'] >= minimum, 'IRQ priority violates FreeRTOS syscall mask')
            require(irq < soc['irq_count'] and irq not in irqs, 'IRQ ownership conflict')
            irqs[irq] = resource['id']
        require(isinstance(resource['pins'], list) and resource['pins'], 'resource pins required')
        seen_signals = set()
        for pin in resource['pins']:
            exact_keys(pin, ('port', 'pin', 'af', 'signal', 'initial'), ('port', 'pin', 'af', 'signal'), 'pin')
            require(pin['port'] in 'ABCDEFGHI' and type(pin['pin']) is int and 0 <= pin['pin'] <= 15, 'invalid pin')
            require(type(pin['af']) is int and 0 <= pin['af'] <= 15, 'invalid pin AF')
            identity = (pin['port'], pin['pin'])
            require(identity not in pins, f'pin ownership conflict: P{pin["port"]}{pin["pin"]}')
            pins[identity] = resource['id']
            require(pin['signal'] not in seen_signals, 'duplicate resource signal')
            seen_signals.add(pin['signal'])
            if route and pin['signal'] in route:
                require((pin['port'], pin['pin'], pin['af']) == route[pin['signal']], 'pin/AF route mismatch')
            elif route:
                require(resource['kind'] == 'spi' and pin['signal'].startswith('cs') and pin['af'] == 0 and pin.get('initial') in (0, 1), 'unreviewed peripheral signal')
            else:
                require(pin['af'] == 0 and pin.get('initial') in (0, 1), 'GPIO needs neutral AF and explicit initial level')
                if soc['platform'] == 'stm32':
                    prefix = f'CONFIG_GPIO_{pin["port"]}{pin["pin"]}'
                    if config.get(f'CONFIG_INSTANCE_STM32_GPIO{pin["port"]}_PIN{pin["pin"]}') is True:
                        require(number(config.get(prefix + '_INIT_VALUE', 0), prefix) == pin['initial'], 'GPIO initial level/config conflict')
        if route:
            require(set(route) <= seen_signals, 'missing required controller pin signal')
        for channel in resource.get('dma', []):
            exact_keys(channel, ('direction', 'controller', 'stream', 'channel', 'irq'),
                       ('direction', 'controller', 'stream', 'channel', 'irq'), 'DMA')
            require(key in DMA_ROUTES and channel['direction'] in DMA_ROUTES[key], 'unsupported DMA route')
            require((channel['controller'], channel['stream'], channel['channel'], channel['irq']) == DMA_ROUTES[key][channel['direction']], 'DMA route mismatch')
            slot = (channel['controller'], channel['stream'])
            require(slot not in dma and channel['irq'] not in irqs, 'DMA/IRQ ownership conflict')
            dma[slot] = resource['id']
            irqs[channel['irq']] = resource['id']
        active.append(resource)
    # Every enabled maintained controller needs an explicit Board binding.
    for (platform, kind, instance) in ROUTES:
        if platform != soc['platform']:
            continue
        if platform == 'stm32':
            symbol = f'CONFIG_INSTANCE_STM32_UART_{instance}' if kind == 'uart' else f'CONFIG_STM32_SPI{instance}_ENABLE'
        else:
            symbol = f'CONFIG_GD32_{kind.upper()}{instance}_ENABLE'
        require(config.get(symbol) is not True or (platform, kind, instance) in controllers, f'enabled controller missing Board resource: {symbol}')
    if soc['platform'] != 'native':
        require(number(config['CONFIG_LINKER_FLASH_SIZE'], 'physical Flash size') == soc['flash_size'], 'SoC/effective physical Flash size conflict')
        require(number(config['CONFIG_LINKER_RAM_SIZE'], 'physical RAM size') == soc['ram_size'], 'SoC/effective physical RAM size conflict')
    if soc['platform'] == 'stm32':
        for symbol, enabled in config.items():
            match = re.fullmatch(r'CONFIG_INSTANCE_STM32_GPIO([A-I])_PIN([0-9]+)', symbol)
            if match and enabled is True:
                pin = (match[1], int(match[2]))
                require(pin in pins, f'enabled GPIO missing Board resource: {symbol}')
                owner = next(resource for resource in active if resource['id'] == pins[pin])
                require(owner['kind'] == 'gpio', f'GPIO/controller pin ownership conflict: {symbol}')
        for kind, symbol in [('uart', 'CONFIG_INSTANCE_STM32_UART_'), ('spi', 'CONFIG_STM32_SPI')]:
            for name, enabled in config.items():
                pattern = symbol + (r'([0-9]+)' if kind == 'uart' else r'([0-9]+)_ENABLE')
                match = re.fullmatch(pattern, name)
                if match and enabled is True:
                    require(('stm32', kind, int(match[1])) in controllers, f'enabled controller missing Board resource: {name}')
        if config.get('CONFIG_STM32_SPI_USE_DMA') is True:
            for resource in active:
                if resource['kind'] == 'spi':
                    require({entry['direction'] for entry in resource.get('dma', [])} == {'tx', 'rx'}, 'SPI DMA enabled without complete Board bindings')
    identity = {'manifest_sha256': digest_file(root / 'board.json'), 'inputs_sha256': inputs}
    identity['sha256'] = hashlib.sha256(json.dumps(identity, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    return raw, soc, identity, active


def block_boundaries(soc_name):
    size = SOCS[soc_name]['flash_size']
    if soc_name.startswith('stm32'):
        sizes = [0x4000] * 4 + [0x10000] + [0x20000] * (7 if size == 0x100000 else 3)
        boundaries = [0]
        for block in sizes:
            boundaries.append(boundaries[-1] + block)
        return set(boundaries)
    return set(range(0, size + 1, 0x1000))


def validate_layout(path, soc_name):
    soc = SOCS[soc_name]
    if not soc['flash_size']:
        require(path is None, 'Native Board cannot select a physical Flash layout')
        return None
    raw = json.loads(Path(path).read_text()) if path else {'schema': 1, 'soc': soc_name, 'image': {'offset': 0, 'size': soc['flash_size']}, 'regions': []}
    exact_keys(raw, ('schema', 'soc', 'image', 'regions'), ('schema', 'soc', 'image', 'regions'), 'layout')
    require(raw['schema'] == 1 and raw['soc'] == soc_name, 'layout schema/SoC conflict')
    require(isinstance(raw['regions'], list), 'layout regions must be a list')
    regions, spans, names = [], [], set()
    for index, region in enumerate([raw['image'], *raw['regions']]):
        exact_keys(region, ('offset', 'size', 'name'), ('offset', 'size') if index == 0 else ('offset', 'size', 'name'), 'layout region')
        offset, size = number(region['offset'], 'offset'), number(region['size'], 'size')
        require(size and offset + size <= soc['flash_size'], 'layout range outside physical Flash/overflow')
        require(offset in block_boundaries(soc_name) and offset + size in block_boundaries(soc_name), 'layout boundary crosses physical erase block')
        require(not any(offset < end and start < offset + size for start, end in spans), 'layout region/image overlap')
        spans.append((offset, offset + size))
        name = 'image' if index == 0 else region['name']
        require(isinstance(name, str) and re.fullmatch(r'[a-z][a-z0-9_]{0,47}', name) and name not in names, 'invalid/duplicate layout region name')
        names.add(name)
        regions.append({'name': name, 'offset': offset, 'size': size})
    # Relocated images require a boot/vector-remap contract, outside this SDK path.
    require(regions[0]['offset'] == 0, 'nonzero image origin requires an external verified boot/startup binding')
    resolved = {'schema': 1, 'soc': soc_name, 'flash_base': 0x08000000,
                'flash_size': soc['flash_size'], 'image': {k: regions[0][k] for k in ('offset', 'size')}, 'regions': regions[1:]}
    resolved['sha256'] = hashlib.sha256(json.dumps(resolved, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    resolved['input_sha256'] = digest_file(Path(path)) if path else None
    return resolved


def write_if_changed(path, contents):
    if not path.exists() or path.read_text() != contents:
        path.write_text(contents)


def emit(directory, manifest, soc, identity, active, layout, sdk_root):
    out = Path(directory)
    out.mkdir(parents=True, exist_ok=True)
    write_if_changed(out / 'board-identity.json', json.dumps({'schema': 1, 'id': manifest['id'], 'soc': manifest['soc'], **identity, 'active_resources': active}, indent=2, sort_keys=True) + '\n')
    cmake = [f'set(NEXUS_BOARD_NAME [=[{manifest["id"]}]=])', f'set(NEXUS_BOARD_SHA256 {identity["sha256"]})', f'set(NEXUS_BOARD_TARGET {manifest["interface_target"]})', f'set(NEXUS_BOARD_OBJECT_TARGETS "{";".join(manifest["object_targets"])}")']
    if layout:
        write_if_changed(out / 'layout.json', json.dumps(layout, indent=2, sort_keys=True) + '\n')
        macros = ['#ifndef NEXUS_FLASH_LAYOUT_H', '#define NEXUS_FLASH_LAYOUT_H', '#include <stdint.h>', f'#define NX_LAYOUT_SHA256 "{layout["sha256"]}"', f'#define NX_LAYOUT_IMAGE_OFFSET UINT32_C({layout["image"]["offset"]})', f'#define NX_LAYOUT_IMAGE_SIZE UINT32_C({layout["image"]["size"]})', f'#define NX_LAYOUT_REGION_COUNT {len(layout["regions"])}U']
        entries = ' '.join(f'X({r["name"]}, UINT32_C({r["offset"]}), UINT32_C({r["size"]}))' for r in layout['regions'])
        macros.extend([f'#define NX_LAYOUT_REGION_LIST(X) {entries}', '#endif', ''])
        write_if_changed(out / 'nx_flash_layout.h', '\n'.join(macros))
        stm32 = soc['platform'] == 'stm32'
        extra = 'CCMRAM (rw) : ORIGIN = 0x10000000, LENGTH = 64K' if stm32 else 'ADDRAM (rw) : ORIGIN = 0x20030000, LENGTH = 256K\nTCM (rw) : ORIGIN = 0x10000000, LENGTH = 64K'
        script = f'ENTRY(Reset_Handler)\nMEMORY {{\nFLASH (rx) : ORIGIN = 0x08000000, LENGTH = {layout["image"]["size"]}\nRAM (rw) : ORIGIN = 0x20000000, LENGTH = {soc["ram_size"]}\n{extra}\n}}\n_estack = ORIGIN(RAM) + LENGTH(RAM);\nPROVIDE(_Min_Stack_Size = 0x1000);\nPROVIDE(_Min_Heap_Size = 0x2000);\n'
        script += f'__nexus_image_start = 0x08000000;\n__nexus_image_end = 0x{0x08000000 + layout["image"]["size"]:08x};\n'
        for index in range(8):
            script += f'__nexus_layout_sha256_{index} = 0x{layout["sha256"][index * 8:(index + 1) * 8]};\n'
        for index in range(8):
            script += f'__nexus_board_sha256_{index} = 0x{identity["sha256"][index * 8:(index + 1) * 8]};\n'
        for region in layout['regions']:
            script += f'__nexus_region_{region["name"]}_start = 0x{0x08000000 + region["offset"]:08x};\n__nexus_region_{region["name"]}_end = 0x{0x08000000 + region["offset"] + region["size"]:08x};\n'
        if not stm32:
            script += '_sp = _estack;\nEXTERN(nx_device_descriptor_bytes)\n'
        sections = 'soc/stm32f407/linker/stm32f407_sections.ld' if stm32 else 'soc/gd32f470/linker/gd32f470_sections.ld'
        script += f'INCLUDE "{(Path(sdk_root) / sections).as_posix()}"\n'
        write_if_changed(out / 'firmware.ld', script)
        cmake.extend([f'set(NEXUS_GENERATED_LINKER_SCRIPT "{(out / "firmware.ld").as_posix()}")', f'set(NEXUS_LAYOUT_SHA256 {layout["sha256"]})'])
    write_if_changed(out / 'board.cmake', '\n'.join(cmake) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board-dir', type=Path, required=True)
    parser.add_argument('--config', type=Path, required=True)
    parser.add_argument('--layout', type=Path)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--sdk-root', type=Path, required=True)
    args = parser.parse_args()
    try:
        manifest, soc, identity, active = validate_manifest(args.board_dir, config_values(args.config))
        layout = validate_layout(args.layout, manifest['soc'])
        emit(args.output_dir, manifest, soc, identity, active, layout, args.sdk_root)
    except (ValueError, OSError, KeyError, TypeError, json.JSONDecodeError) as error:
        # A rejected reconfiguration must not leave a consumable old Board/layout bundle.
        for name in ('board.cmake', 'board-identity.json', 'layout.json', 'nx_flash_layout.h', 'firmware.ld'):
            (args.output_dir / name).unlink(missing_ok=True)
        print(f'Board/layout validation failed: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
