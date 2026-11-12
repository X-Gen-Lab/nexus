"""One static F407 FSMC owner; concrete C owns all MMIO and bank behavior."""
from types import MappingProxyType

from tools.configure import board_hardware
from tools.configure.calls import Address, DirectCall, InitializeSequence, Symbol
from tools.configure.providers.common import Construction, fail, integer


BANKS = MappingProxyType({
    "memory_window": ("fsmc-ne3-is62wv51216", "cpu-window", 3),
    "parallel16": ("fsmc-ne4-ili9341", "8080-16", 4),
})
TIMING_FIELDS = frozenset(("address_setup", "address_hold", "data_setup",
                          "bus_turn"))


def _timing(value):
    if not isinstance(value, dict) or set(value) != TIMING_FIELDS:
        fail("FSMC timing requires all four encoded LL fields")
    for field, low, high in (("address_setup", 0, 15),
                             ("address_hold", 1, 15),
                             ("data_setup", 1, 255), ("bus_turn", 0, 15)):
        integer(value[field], low, high, "FSMC " + field)


def resolve_endpoint(device, controller, *, authored_kind, bindings, routes,
                     claim, board_conditions, clock=None):
    """Admit an exact bank without making a second shared GPIO owner."""
    if authored_kind in {'nand','flash_async'} or device.get('mode')=='psram-window':
        return _reference_resolve(device,controller,authored_kind,bindings,routes,
                                  claim,board_conditions)
    if 'parameters' in device: fail('Legacy SRAM/LCD rejects generic parameters')
    if (authored_kind not in BANKS or
            device["driver"] != authored_kind + "-endpoint"):
        fail("FSMC requires a reviewed memory_window or parallel16 endpoint")
    expected_route, mode, bank = BANKS[authored_kind]
    if authored_kind == "memory_window":
        mode = device["mode"]
        if mode not in {"cpu-window", "startup-memory"}:
            fail("Memory bank requires explicit cpu-window or "
                 "startup-memory mode")
    binding = bindings.get(device["binding"])
    if binding is None:
        fail("Unknown external bank Board binding")
    board_hardware.require_binding(binding, board_conditions)
    route = routes[binding["route"]]
    if (controller["controller"] != "FSMC" or
            route["controller"] != controller["controller"] or
            route["kind"] != authored_kind or route["id"] != expected_route or
            mode not in route["modes"] or
            any(pin["af"] != 12 for pin in route["pins"])):
        fail("External bank differs from its exact reviewed FSMC route")
    _timing(device["read_timing"])
    _timing(device["write_timing"])
    claim("ip:FSMC/bank" + str(bank), device["id"], controller["mode"],
          route["id"])
    if authored_kind == "memory_window":
        device.update(address=0x68000000, bytes=0x100000)
    else:
        device.update(command_address=0x6c000000, data_address=0x6c002000)


def complete_controller(controller, children, *, claim, bindings, routes):
    """Publish exactly one bus pin union after all banks have been admitted."""
    if not children:
        fail("External bus requires at least one explicit bank endpoint")
    selected = {}
    for child in children:
        route = routes[bindings[child["binding"]]["route"]]
        parameters=child.get('parameters')
        pads=(route['pins'] if parameters is None else
              _reference_pins(child['driver'].removesuffix('-endpoint'),parameters,route))
        for pin in pads:
            previous = selected.get(pin["pin"])
            if previous is not None and previous != pin:
                fail("FSMC shared pad has inconsistent selected route facts")
            selected[pin["pin"]] = pin
    pins = [selected[name] for name in sorted(
        selected, key=lambda name: (name[1], int(name[2:])))]
    controller["pins"] = pins
    for pin in pins:
        claim("pin:" + pin["pin"], controller["id"], controller["mode"],
              controller["route"])


def _encoded(timing):
    return "{ " + ", ".join(str(getattr(timing, key)) + "u" for key in
                            ("address_setup", "address_hold", "data_setup",
                             "bus_turn")) + " }"


def _startup_endpoint(result):
    children = [child for child in result.devices
                if child.driver == "memory_window-endpoint" and
                child.options.mode == "startup-memory"]
    if len(children) > 1:
        fail("Only one exact SRAM startup bank is maintained")
    return children[0] if children else None


def boot_declarations(result):
    """Declare the same readonly bank array used by reset and runtime."""
    child = _startup_endpoint(result)
    if child is None:
        return []
    name = child.controller.replace("-", "_")
    return ["extern const nx_stm32_fsmc_bank_config_t "
            f"g_nx_fsmc_bank_config_{name}[];",
            "extern const nx_stm32_sram_boot_plan_t "
            "g_nx_stm32_sram_boot_plan;"]


def boot_bank_reference(result):
    """Return the exact immutable descriptor member, never a copied bank."""
    child = _startup_endpoint(result)
    if child is None:
        fail("External SRAM startup plan requires an explicit selected bank")
    children = [item for item in result.devices
                if item.controller == child.controller]
    index = children.index(child)
    return ("&g_nx_fsmc_bank_config_" + child.controller.replace("-", "_") +
            f"[{index}u]")


def excluded_pins(result):
    """Keep reset-owned SRAM pads outside the ordinary cold restore lease."""
    child = _startup_endpoint(result)
    if child is None:
        return ()
    route = result.routes[result.board_bindings[child.options.binding]["route"]]
    return tuple(pin["pin"] for pin in route["pins"])


def constructor(item, result):
    """Emit readonly faces and fixed C call order, with exact bank storage."""
    if item.mode=='external-memory':
        return _reference_constructor(item,result)
    if (item.controller != "FSMC" or item.mode != "async16" or item.dma or
            result.part != "STM32F407ZGT6"):
        fail("FSMC requires the reviewed exact F407ZG assembly")
    name = item.id.replace("-", "_")
    children = [child for child in result.devices if child.controller == item.id]
    boot = any(child.driver == "memory_window-endpoint" and
               child.options.mode == "startup-memory" for child in children)
    retained = set(excluded_pins(result)) if boot else set()
    pins = "s_nx_fsmc_pins_" + name
    saved = "s_nx_fsmc_pin_state_" + name
    banks = ("g_nx_" if boot else "s_nx_") + "fsmc_bank_config_" + name
    runtime = "s_nx_fsmc_banks_" + name
    config = "s_nx_fsmc_config_" + name
    bus = "s_nx_port_" + name
    count = len(item.pins) - len(retained)
    definitions = ([f"static uint16_t {saved}[{count}u];"] if count else []) + [
        f"static const nx_stm32_platform_pin_config_t {pins}[] = {{"]
    snapshots = {}
    for pin in item.pins:
        wire = pin["pin"]
        slot = f"{saved}[{len(snapshots)}u]"
        address = "NULL" if wire in retained else "&" + slot
        if wire not in retained:
            snapshots[wire] = slot
        definitions.append(f"    {{ GPIO{wire[1]}, {address}, "
                           f"{ord(wire[1]) - ord('A')}u, {int(wire[2:])}u }},")
    storage = "" if boot else "static "
    definitions += ["};", f"{storage}const nx_stm32_fsmc_bank_config_t {banks}[] = {{"]
    for child in children:
        kind = ("NX_STM32_FSMC_SRAM_NE3" if child.driver == "memory_window-endpoint"
                else "NX_STM32_FSMC_LCD_NE4")
        definitions.append(f"    {{ {kind}, {_encoded(child.options.read_timing)}, "
                           f"{_encoded(child.options.write_timing)} }},")
    definitions += ["};", "static nx_stm32_fsmc_bank_state_t "
                    f"{runtime}[{len(children)}u];",
                    f"static const nx_stm32_fsmc_config_t {config} = {{",
                    f"    .pins = {pins}, .pin_count = {len(item.pins)}u,",
                    f"    .banks = {banks}, .bank_count = {len(children)}u,",
                    "    .clock = { nx_stm32_system_clock_read, &g_nx_stm32_system },",
                    f"    .boot_backed = {str(boot).lower()}", "};"]
    if boot:
        definitions += ["extern const nx_stm32_sram_boot_plan_t "
                        "g_nx_stm32_sram_boot_plan;"]
    arguments = (Address(bus), Address(config), Symbol(runtime), len(children))
    prepare = "nx_stm32_fsmc_adopt_boot" if boot else "nx_stm32_fsmc_prepare"
    if boot:
        arguments += (Address("g_nx_stm32_sram_boot_plan"),)
    start = [DirectCall(prepare, arguments)]
    stop = []
    for child in children:
        endpoint = child.id.replace("-", "_")
        if child.driver == "memory_window-endpoint":
            typename, context = "nx_memory_window_port_t", bus
            operations = "nx_stm32_fsmc_memory_window_ops"
        else:
            typename = "nx_parallel16_endpoint_t"
            context = "s_nx_fsmc_lcd_" + endpoint
            operations = "nx_stm32_fsmc_parallel16_ops"
            definitions.append(f"static nx_stm32_fsmc_lcd_t {context};")
            start.append(DirectCall("nx_stm32_fsmc_lcd_prepare", (
                Address(bus), Address(context))))
            stop.append(DirectCall("nx_stm32_fsmc_lcd_close", (Address(context),)))
        definitions += [f"static const {typename} s_nx_device_face_{endpoint} = {{",
                        f"    &{operations}, &{context}", "};",
                        f"const {typename}* const nx_device_{endpoint} = "
                        f"&s_nx_device_face_{endpoint};"]
    stop.append(DirectCall("nx_stm32_fsmc_stop", (Address(bus),)))
    sequence = InitializeSequence("prepare_fsmc_" + name, tuple(start))
    return Construction(state_type="nx_stm32_fsmc_t", definitions=definitions,
                        initialize_sequences=(sequence,),
                        initialize=DirectCall(sequence.function), stop=stop,
                        retain_on_failed_init=True, pin_snapshots=snapshots,
                        initial_state=(Symbol("NX_STM32_FSMC_BOOT_OWNER")
                                       if boot else None),
                        pre_c_effects=boot)

# Software-reference pads are a reviewed chip inventory, not a PCB assertion.
_DATA = ('PD14','PD15','PD0','PD1','PE7','PE8','PE9','PE10','PE11',
         'PE12','PE13','PE14','PE15','PD8','PD9','PD10')
_ADDRESS = ('PF0','PF1','PF2','PF3','PF4','PF5','PF12','PF13','PF14','PF15',
            'PG0','PG1','PG2','PG3','PG4','PG5','PD11','PD12','PD13','PE3',
            'PE4','PE5','PE6','PE2','PG13','PG14')
_NS_FIELDS = frozenset(('bank','width','address_lines','bytes','multiplexed',
    'byte_lanes','burst_read','burst_write','clock_division','data_latency',
    'access_mode','wait_signal','asynchronous_wait','wait_high','wait_before','wrap',
    'extended_mode','burst_page_bytes'))
_NAND_FIELDS = frozenset(('bank','width','tclr','tar','ecc_bytes','wait_signal',
                          'common','attribute'))


def _exact(value, fields, label):
    if not isinstance(value, dict) or set(value) != fields:
        fail(label + ' requires exact explicit fields ' + str(sorted(fields)))


def _reference_parameters(p, nand):
    _exact(p, _NAND_FIELDS if nand else _NS_FIELDS, 'FSMC parameters')
    integer(p['bank'], 2 if nand else 1, 3 if nand else 4, 'FSMC bank')
    if type(p['width']) is not int or p['width'] not in (8,16):
        fail('FSMC width must be 8 or 16')
    flags = {'multiplexed','extended_mode','byte_lanes','burst_read','burst_write','wait_signal',
             'asynchronous_wait','wait_high','wait_before','wrap'}
    if any(type(p[key]) is not bool for key in flags.intersection(p)):
        fail('FSMC electrical flags require explicit booleans')
    if nand:
        for key in ('tclr','tar'): integer(p[key],0,15,'NAND '+key)
        if type(p['ecc_bytes']) is not int or p['ecc_bytes'] not in (256,512,1024,2048,4096,8192):
            fail('NAND requires an exact hardware ECC chunk')
        for key in ('common','attribute'):
            _exact(p[key], {'setup','wait','hold','hiz'}, 'NAND timing')
            for name,value in p[key].items(): integer(value,0,254,'NAND '+name)
        return
    integer(p['address_lines'],1,26,'FSMC address pads')
    integer(p['bytes'],1,min(0x4000000,(1<<p['address_lines'])*(p['width']//8)), 'FSMC bytes')
    integer(p['clock_division'],2,16,'FSMC clock division')
    integer(p['data_latency'],2,17,'FSMC data latency')
    if p['access_mode'] not in ('A','B','C','D'): fail('FSMC access mode must be A..D')
    if type(p['burst_page_bytes']) is not int or p['burst_page_bytes'] not in (0,128,256,512,1024):
        fail('FSMC burst page bytes must be 0/128/256/512/1024')
    if (p['multiplexed'] and p['extended_mode'] or
        p['burst_page_bytes'] and not p['burst_read'] or
        not p['extended_mode'] and p['access_mode']!='A' or
        p['bytes']%(p['width']//8) or
        (p['burst_write'] or p['wrap']) and not p['burst_read'] or
        p['burst_read'] and 168000000/p['clock_division']>60000000 or
        p['wait_high'] and not (p['wait_signal'] or p['asynchronous_wait']) or
        p['wait_before'] and not p['wait_signal']):
        fail('FSMC geometry/burst/WAIT choices are inconsistent')


def _reference_pins(kind, p, route):
    pads=set(_DATA[:p['width']]) | {'PD4','PD5'}
    if kind=='nand':
        pads.update(('PD11','PD12','PD7' if p['bank']==2 else 'PG9'))
    else:
        pads.add({1:'PD7',2:'PG9',3:'PG10',4:'PG12'}[p['bank']])
        pads.update(_ADDRESS[p['width'] if p['multiplexed'] else 0:p['address_lines']])
        if p['multiplexed']: pads.add('PB7')
        if p['byte_lanes']: pads.update(('PE0','PE1') if p['width']==16 else ('PE0',))
        if p['burst_read']: pads.add('PD3')
    if p['wait_signal'] or p.get('asynchronous_wait',False): pads.add('PD6')
    available={pin['pin']:pin for pin in route['pins']}
    if not pads.issubset(available): fail('FSMC pads unavailable on exact package/route')
    return [available[name] for name in sorted(pads)]


def _reference_protocol(device, kind, p):
    if kind=='nand':
        g=device['geometry']
        _exact(g, {'page_bytes','oob_bytes','pages_per_block','blocks',
                   'column_cycles','row_cycles'}, 'NAND geometry')
        if type(g['page_bytes']) is not int or g['page_bytes'] not in (512,1024,2048,4096,8192):
            fail('Unsupported raw NAND page bytes')
        for key,high in (('oob_bytes',1024),('pages_per_block',65536),('blocks',0xffffff)):
            integer(g[key],1,high,'NAND '+key)
        integer(g['row_cycles'],2,3,'NAND row cycles')
        if (g['column_cycles']!=(1 if g['page_bytes']==512 else 2) or
            g['blocks']*g['pages_per_block']>1<<(8*g['row_cycles']) or
            g['oob_bytes']%(p['width']//8) or p['ecc_bytes']>g['page_bytes'] or
            g['page_bytes']%p['ecc_bytes']): fail('Inconsistent NAND address/ECC geometry')
    elif kind=='flash_async':
        n=device['nor']
        _exact(n, {'command_set','cfi','unlock1','unlock2','cfi_address','erase_spans','ready_delay_us','requires_fresh_erase'}, 'NOR protocol')
        if type(n['command_set']) is not int or n['command_set'] not in (1,2) or type(n['cfi']) is not bool:
            fail('NOR requires explicit CFI choice and command set 1/2')
        integer(n['ready_delay_us'],1,1000000,'NOR busy assertion bound')
        if type(n['requires_fresh_erase']) is not bool:
            fail('NOR erase provenance rule must be explicit')
        if p['burst_read'] or p['burst_write'] or p['burst_page_bytes']:
            fail('Finite NOR command provider requires asynchronous bus cycles')
        for key in ('unlock1','unlock2','cfi_address'):
            integer(n[key],0,p['bytes']-1,'NOR command byte offset')
            if n[key]%(p['width']//8): fail('Misaligned NOR byte address')
        from tools.configure.memory import flash_erase_spans
        flash_erase_spans({"size": p["bytes"], "erase_spans": n["erase_spans"]},
                          program_unit=p["width"] // 8)


def _reference_resolve(device, controller, kind, bindings, routes, claim, conditions):
    binding=bindings.get(device['binding'])
    if binding is None: fail('Unknown reference external bank binding')
    board_hardware.require_binding(binding,conditions)
    route=routes[binding['route']]
    mode={'nand':'raw-page','flash_async':'nor-cfi','memory_window':'psram-window'}[kind]
    if (controller['controller']!='FSMC' or controller['mode']!='external-memory' or
        route['controller']!='FSMC' or route['kind']!=kind or mode not in route['modes'] or
        not route['id'].startswith('fsmc-reference-') or any(pin['af']!=12 for pin in route['pins'])):
        fail('Reference bank differs from explicit FSMC software route')
    p=device.get('parameters')
    _reference_parameters(p,kind=='nand')
    _reference_pins(kind,p,route)
    _reference_protocol(device,kind,p)
    if kind=='nand':
        integer(device['ready_delay_us'],1,1000000,'NAND busy assertion bound')
        if route['id']!='fsmc-reference-nand'+str(p['bank']): fail('NAND bank differs from route')
        device.update(address=0x70000000+(p['bank']-2)*0x10000000,bytes=0x40000)
        cs='PD7' if p['bank']==2 else 'PG9'
    else:
        _timing(device['read_timing']);_timing(device['write_timing'])
        if not p['extended_mode'] and device['read_timing']!=device['write_timing']:
            fail('Disabled EXTMOD requires identical read/write timing')
        device.update(address=0x60000000+(p['bank']-1)*0x4000000,bytes=p['bytes'])
        cs={1:'PD7',2:'PG9',3:'PG10',4:'PG12'}[p['bank']]
        p['access_mode']='ABCD'.index(p['access_mode'])
    claim('ip:FSMC/'+('nand' if kind=='nand' else 'bank')+str(p['bank']),device['id'],controller['mode'],route['id'])
    claim('ip:FSMC/cs-'+cs,device['id'],controller['mode'],route['id'])
    device['mode']=mode


def _reference_constructor(item,result):
    """Emit selected storage/typed calls only; all hardware behavior is C11."""
    from dataclasses import fields
    if item.controller!='FSMC' or item.dma or result.part not in ('STM32F407VET6','STM32F407ZGT6'):
        fail('Reference FSMC requires an exact maintained F407 part')
    children=[child for child in result.devices if child.controller==item.id]
    if any(child.driver=='memory_window-endpoint' and child.options.mode=='startup-memory' for child in children):
        fail('Reference banks do not provide C-runtime startup backing')
    name=item.id.replace('-','_')
    bus='s_nx_port_'+name
    saved='s_nx_fsmc_pin_state_'+name
    pins='s_nx_fsmc_pins_'+name
    banks='s_nx_fsmc_bank_config_'+name
    parameters='s_nx_fsmc_parameters_'+name
    runtime='s_nx_fsmc_banks_'+name
    config='s_nx_fsmc_config_'+name
    definitions=[f'static uint16_t {saved}[{len(item.pins)}u];',
                 f'static const nx_stm32_platform_pin_config_t {pins}[] = {{']
    snapshots={}
    for index,pin in enumerate(item.pins):
        wire=pin['pin'];slot=f'{saved}[{index}u]';snapshots[wire]=slot
        definitions.append(f'    {{ GPIO{wire[1]}, &{slot}, {ord(wire[1])-65}u, {int(wire[2:])}u }},')
    definitions += ['};',f'static const nx_stm32_fsmc_bank_config_t {banks}[] = {{']
    for child in children:
        p=getattr(child.options,'parameters',None)
        kind=({'memory_window-endpoint':'PSRAM','flash_async-endpoint':'NOR','nand-endpoint':'NAND'}[child.driver]
              if p is not None else 'SRAM_NE3' if child.driver=='memory_window-endpoint' else 'LCD_NE4')
        read=write='{ 0u, 1u, 1u, 0u }'
        if child.driver!='nand-endpoint':
            read=_encoded(child.options.read_timing);write=_encoded(child.options.write_timing)
        definitions.append(f'    {{ NX_STM32_FSMC_{kind}, {read}, {write} }},')
    definitions += ['};',f'static const nx_stm32_fsmc_parameters_t {parameters}[] = {{']
    for child in children:
        p=getattr(child.options,'parameters',None)
        if p is None:
            definitions.append('    {0},');continue
        encoded=[]
        for field in fields(p):
            value=getattr(p,field.name)
            text=(str(value).lower() if type(value) is bool else
                  '{ '+', '.join(str(getattr(value,key))+'u' for key in ('setup','wait','hold','hiz'))+' }'
                  if field.name in ('common','attribute') else str(value)+'u')
            encoded.append('.'+field.name+' = '+text)
        definitions.append('    { '+', '.join(encoded)+' },')
    definitions += ['};',f'static nx_stm32_fsmc_bank_state_t {runtime}[{len(children)}u];',
        f'static const nx_stm32_fsmc_config_t {config} = {{',
        f'    .pins = {pins}, .pin_count = {len(item.pins)}u,',
        f'    .banks = {banks}, .bank_count = {len(children)}u,',
        '    .clock = { nx_stm32_system_clock_read, &g_nx_stm32_system },',
        f'    .boot_backed = false, .parameters = {parameters},',
        f'    .package_pins = {100 if result.part=="STM32F407VET6" else 144}u','};']
    start=[DirectCall('nx_stm32_fsmc_prepare',(Address(bus),Address(config),Symbol(runtime),len(children)))]
    stop=[]
    for index,child in enumerate(children):
        endpoint=child.id.replace('-','_');p=getattr(child.options,'parameters',None)
        if child.driver in ('flash_async-endpoint','nand-endpoint'):
            nor=child.driver=='flash_async-endpoint';suffix='nor' if nor else 'nand'
            context='s_nx_fsmc_'+suffix+'_'+endpoint
            facts='s_nx_fsmc_'+suffix+'_config_'+endpoint
            geometry='s_nx_fsmc_geometry_'+endpoint
            if nor:
                spans='s_nx_fsmc_erase_spans_'+endpoint;protocol=child.options.nor
                definitions += [f'static const nx_flash_erase_span_t {spans}[] = {{']
                definitions += [f'    {{ {span.offset}u, {span.erase_bytes}u, {span.count}u }},' for span in protocol.erase_spans]
                definitions += ['};',f'static const nx_flash_geometry_t {geometry} = {{',
                    f'    {child.options.address}u, {p.bytes}u, {p.width//8}u, {spans}, {len(protocol.erase_spans)}u, {"NX_FLASH_PROGRAM_REQUIRES_FRESH_ERASE" if protocol.requires_fresh_erase else "0u"}','};',
                    f'static const nx_stm32_fsmc_nor_config_t {facts} = {{',
                    f'    &{geometry}, {protocol.unlock1}u, {protocol.unlock2}u, {protocol.cfi_address}u, {protocol.command_set}u, {str(protocol.cfi).lower()}, {protocol.ready_delay_us}u','};']
                typename='nx_flash_async_port_t';operations='nx_stm32_fsmc_nor_async_ops'
            else:
                g=child.options.geometry
                definitions += [f'static const nx_nand_geometry_t {geometry} = {{',
                    f'    {g.page_bytes}u, {g.oob_bytes}u, {g.pages_per_block}u, {g.blocks}u, {p.ecc_bytes}u, {p.width}u, {g.column_cycles}u, {g.row_cycles}u','};',
                    f'static const nx_stm32_fsmc_nand_config_t {facts} = {{ &{geometry}, {child.options.ready_delay_us}u }};']
                typename='nx_nand_port_t';operations='nx_stm32_fsmc_nand_ops'
            definitions.append(f'static nx_stm32_fsmc_{suffix}_t {context};')
            if nor:
                definitions += [f'static const nx_flash_port_t s_nx_flash_readonly_{endpoint} = {{',
                    f'    &nx_stm32_fsmc_nor_flash_ops, &{context}','};']
            start.append(DirectCall('nx_stm32_fsmc_'+suffix+'_prepare',(Address(context),Address(bus),index,Address(facts))))
            stop.append(DirectCall('nx_stm32_fsmc_'+suffix+'_close',(Address(context),)))
        elif child.driver=='memory_window-endpoint':
            typename='nx_memory_window_port_t';context=bus;operations='nx_stm32_fsmc_memory_window_ops'
            if p is not None:
                context='s_nx_fsmc_window_'+endpoint;operations='nx_stm32_fsmc_window_ops'
                definitions.append(f'static nx_stm32_fsmc_window_t {context} = {{ &{bus}, {index}u }};')
        else:
            typename='nx_parallel16_endpoint_t';context='s_nx_fsmc_lcd_'+endpoint
            operations='nx_stm32_fsmc_parallel16_ops'
            definitions.append(f'static nx_stm32_fsmc_lcd_t {context};')
            start.append(DirectCall('nx_stm32_fsmc_lcd_prepare',(Address(bus),Address(context))))
            stop.append(DirectCall('nx_stm32_fsmc_lcd_close',(Address(context),)))
        definitions += [f'static const {typename} s_nx_device_face_{endpoint} = {{',
            f'    &{operations}, &{context}','};',
            f'const {typename}* const nx_device_{endpoint} = &s_nx_device_face_{endpoint};']
    stop.append(DirectCall('nx_stm32_fsmc_stop',(Address(bus),)))
    sequence=InitializeSequence('prepare_fsmc_'+name,tuple(start))
    return Construction(state_type='nx_stm32_fsmc_t',definitions=definitions,
        initialize_sequences=(sequence,),initialize=DirectCall(sequence.function),
        stop=stop,retain_on_failed_init=True,pin_snapshots=snapshots)
