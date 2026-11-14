"""Reviewed asynchronous banked-memory IP and NOR/NAND protocol admission."""
from tools.configure.providers.common import fail, integer

TIMING_FIELDS = frozenset(("address_setup", "address_hold", "data_setup",
                          "bus_turn"))

def validate_timing(value):
    if not isinstance(value, dict) or set(value) != TIMING_FIELDS:
        fail("FSMC timing requires all four encoded LL fields")
    for field, low, high in (("address_setup", 0, 15),
                             ("address_hold", 1, 15),
                             ("data_setup", 1, 255), ("bus_turn", 0, 15)):
        integer(value[field], low, high, "FSMC " + field)

_NS_FIELDS = frozenset(('bank','width','address_lines','bytes','multiplexed',
    'byte_lanes','burst_read','burst_write','clock_division','data_latency',
    'access_mode','wait_signal','asynchronous_wait','wait_high','wait_before','wrap',
    'extended_mode','burst_page_bytes'))

_NAND_FIELDS = frozenset(('bank','width','tclr','tar','ecc_bytes','wait_signal',
                          'common','attribute'))

def _exact(value, fields, label):
    if not isinstance(value, dict) or set(value) != fields:
        fail(label + ' requires exact explicit fields ' + str(sorted(fields)))

def validate_parameters(p, nand, *, hclk_hz, max_burst_hz):
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
        p['burst_read'] and hclk_hz > max_burst_hz*p['clock_division'] or
        p['wait_high'] and not (p['wait_signal'] or p['asynchronous_wait']) or
        p['wait_before'] and not p['wait_signal']):
        fail('FSMC geometry/burst/WAIT choices are inconsistent')

def validate_protocol(device, kind, p):
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
