#!/usr/bin/env python3
"""Admit one reviewed Board/build/station pair; default is a dry-run.

Reference fixtures contain no observed PCB/probe identity. No Flash, key,
update, power-fail or DMA operation is inferred from a passing host model.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import re
import stat
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'ci'))
from scripts.evidence.common import (EvidenceError,atomic_json,command,digest,fields,
    file_identity,identifier,load_json,regular_file,reject_secret_fields,sha256_value,verify_file_identity)
from scripts.configure.board_package import (validate_manifest as validate_board,
    config_values,validate_layout)
from scripts.hil.run_hil import execute,validate_manifest as validate_hil,utc_now
from scripts.ci.validate_firmware_elf import Elf32

ROOT=Path(__file__).resolve().parents[2]

def require(condition,message):
    if not condition: raise EvidenceError(message)

def _canonical(value):
    return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode()).hexdigest()

def validate_fixture(fixture):
    fields(fixture,{'schema','kind','hardware_verified','board_id','soc','board_manifest',
        'chip_part','flash_base','flash_size','debug','console','led','electrical_notes',
        'required_tests','pending_qualification'})
    require(fixture['schema']==1 and fixture['kind']=='board_fixture_template',
            'unsupported fixture schema/kind')
    require(fixture['hardware_verified'] is False,'reference fixture cannot claim hardware verification')
    for key in ('board_id','soc','chip_part'): identifier(fixture[key],key)
    require(fixture['debug']['transport']=='swd','this fixture requires reviewed SWD transport')
    require(type(fixture['flash_size']) is int and 0<fixture['flash_size']<=1048576,'invalid physical Flash size')
    require(fixture['flash_base']==0x08000000,'unsupported physical Flash base')
    require(fixture['required_tests']==['probe_identity','flash_readback','reset','uart_echo'],
            'smoke acceptance set cannot be weakened')
    for qualification in fixture['pending_qualification']:
        require(qualification['budget'] is None,'unmeasured fixture cannot advertise timing budgets')
    return fixture

def _binary_from_elf(elf,base,limit):
    """Reconstruct file-backed allocated sections at load addresses, no tool call."""
    spans=[]
    for section in elf.sections.values():
        # BSS has no released bytes; allocated debug-less sections are loaded.
        if not section[2]&2 or section[1]==8 or not section[5]: continue
        segment=next((p for p in elf.segments if p[0]==1 and
            p[1]<=section[4] and section[4]+section[5]<=p[1]+p[4]),None)
        require(segment is not None,'allocated ELF section has no load segment')
        address=segment[3]+section[4]-segment[1]
        require(base<=address and section[5]<=limit-address,'ELF load bytes outside selected image')
        spans.append((address,elf.section_data(section)))
    require(bool(spans),'ELF has no physical load bytes')
    require(min(start for start,_ in spans)==base,'firmware binary origin differs from layout image')
    end=max(start+len(data) for start,data in spans)
    result=bytearray(end-base)
    occupied=[]
    for start,data in spans:
        require(not any(start<other_end and other_start<start+len(data)
                        for other_start,other_end in occupied),'ELF load section overlap')
        occupied.append((start,start+len(data)))
        result[start-base:start-base+len(data)]=data
    return bytes(result)

def validate_build(fixture,build_dir,image,static_report):
    require(isinstance(image,str) and identifier(image)==image and '.' not in image,
            'image must be a simple application name without extension')
    generated=build_dir/'generated'
    config=regular_file(generated/'effective.config')
    board_identity=load_json(generated/'board-identity.json')
    layout=load_json(generated/'layout.json')
    board_manifest=Path(fixture['board_manifest'])
    if not board_manifest.is_absolute(): board_manifest=ROOT/board_manifest
    raw,soc,identity,active=validate_board(board_manifest.parent,config_values(config))
    expected={'schema':1,'id':raw['id'],'soc':raw['soc'],**identity,'active_resources':active}
    require(board_identity==expected,'Board generated identity differs from actual package inputs/config')
    require(raw['id']==fixture['board_id'] and raw['soc']==fixture['soc'],'fixture/build Board identity mismatch')
    require(config_values(config).get('CONFIG_BOARD_NAME')==raw['id'],'effective Board name mismatch')
    require(soc['flash_size']==fixture['flash_size'],'fixture/SoC physical density mismatch')
    fields(layout,{'schema','soc','flash_base','flash_size','image','regions','sha256','input_sha256'})
    canonical={key:value for key,value in layout.items() if key not in ('sha256','input_sha256')}
    require(layout['sha256']==_canonical(canonical),'generated layout identity mismatch')
    require(layout['soc']==fixture['soc'] and layout['flash_size']==fixture['flash_size'],
            'layout/fixture SoC mismatch')
    if layout['input_sha256'] is not None:
        sha256_value(layout['input_sha256'])
    # Revalidate exact block boundaries/overlap using the maintained generator.
    import tempfile
    with tempfile.TemporaryDirectory() as temporary:
        raw_layout=Path(temporary)/'layout.json'
        atomic_json(raw_layout,{key:layout[key] for key in ('schema','soc','image','regions')})
        validated=validate_layout(raw_layout,layout['soc'])
        require(validated['sha256']==layout['sha256'],'layout shape/geometry validation mismatch')
    console=next((r for r in active if r['id']=='console'),None)
    require(console is not None,'UART smoke requires the active Board console resource')
    actual_pins={p['signal']:f"P{p['port']}{p['pin']}" for p in console['pins']}
    require(actual_pins.get('tx')==fixture['console']['tx'] and
            actual_pins.get('rx')==fixture['console']['rx'] and
            console['clock']==fixture['console']['controller'],'fixture UART wiring differs from active Board')
    report=load_json(static_report)
    require(report.get('schema_version')==1 and report.get('kind')=='arm-static-link-contract' and
            report.get('hardware_verified') is False,'passing static ARM contract required')
    require(report.get('config_sha256')==digest(config),'static report config identity mismatch')
    require(report.get('board')==fixture['board_id'] or report.get('board_id')==fixture['board_id'],
            'static report Board identity missing/mismatch')
    require(report.get('board_sha256')==identity['sha256'] and report.get('layout_sha256')==layout['sha256'],
            'static report Board/layout identity mismatch')
    elf_path=regular_file(build_dir/'bin'/f'{image}.elf')
    bin_path=regular_file(build_dir/'bin'/f'{image}.bin')
    selected=[item for item in report.get('images',[]) if item.get('file')==elf_path.name]
    require(len(selected)==1 and selected[0].get('sha256')==digest(elf_path),'static ELF image identity mismatch')
    elf=Elf32(elf_path.read_bytes())
    base=layout['flash_base']+layout['image']['offset']
    limit=base+layout['image']['size']
    require(elf.symbol('__nexus_image_start')==base and elf.symbol('__nexus_image_end')==limit,
            'ELF image range differs from layout')
    actual_sha=''.join(f"{elf.symbol(f'__nexus_layout_sha256_{i}'):08x}" for i in range(8))
    require(actual_sha==layout['sha256'],'ELF layout identity mismatch')
    actual_board=''.join(f"{elf.symbol(f'__nexus_board_sha256_{i}'):08x}" for i in range(8))
    require(actual_board==identity['sha256'],'ELF Board identity mismatch')
    for region in layout['regions']:
        require(elf.symbol(f"__nexus_region_{region['name']}_start")==layout['flash_base']+region['offset'] and
                elf.symbol(f"__nexus_region_{region['name']}_end")==layout['flash_base']+region['offset']+region['size'],
                'ELF region boundaries differ from layout')
    require(bin_path.read_bytes()==_binary_from_elf(elf,base,limit),'firmware BIN bytes differ from linked ELF')
    identities=[file_identity(path) for path in (elf_path,bin_path,config,
        generated/'board-identity.json',generated/'layout.json',static_report,board_manifest)]
    for name in raw['inputs']: identities.append(file_identity(board_manifest.parent/name))
    return {'board_sha256':identity['sha256'],'layout_sha256':layout['sha256'],
        'config_sha256':digest(config),'firmware':{'path':str(bin_path),'sha256':digest(bin_path)},
        'elf_sha256':digest(elf_path),'load_address':base,'image_size':bin_path.stat().st_size,
        'artifacts':identities}

def validate_station(station,fixture,*,model=False):
    fields(station,{'schema','kind','lab_id','board','probe','serial','power','outputs_disconnected',
                   'adapter_files','commands','operation_timeout_s','total_timeout_s','budgets'})
    require(station['schema']==1 and station['kind']==('fixture_model' if model else 'lab_station'),
            'station kind does not match execution mode')
    identifier(station['lab_id'],'registered lab id')
    fields(station['board'],{'id','profile','revision','probe_serial','chip_part','chip_uid'})
    for key,value in station['board'].items(): identifier(value,'observed '+key)
    require(station['board']['profile']==fixture['board_id'] and
            station['board']['chip_part']==fixture['chip_part'],'station profile/MCU mismatch')
    require(station['outputs_disconnected'] is True,'disconnect product outputs before smoke HIL')
    fields(station['power'],{'voltage_mv','source'})
    identifier(station['power']['source'],'power source')
    require(type(station['power']['voltage_mv']) is int and 2700<=station['power']['voltage_mv']<=3600,
            'reference board supply must be recorded within2.7..3.6V')
    fields(station['probe'],{'backend','interface','transport','tool','interface_config','target_config'})
    probe=station['probe']
    require(probe['backend'] in ('openocd','external-reviewed'),'unsupported programmer backend')
    require(probe['interface'] in fixture['debug']['allowed_interfaces'] and probe['transport']=='swd',
            'reviewed SWD probe/interface required')
    identities=[]
    for key in ('tool','interface_config','target_config'):
        identity=probe[key]
        if probe['backend']=='external-reviewed' and key!='tool' and identity is None: continue
        verify_file_identity(identity)
        identities.append(identity)
    serial=station['serial']
    fields(serial,{'path','baudrate','electrical_mode','wiring_checked','ground_connected'})
    require(serial['baudrate']==fixture['console']['baudrate'],'serial baudrate differs from fixture')
    require(serial['wiring_checked'] is True and serial['ground_connected'] is True,
            'serial electrical wiring/common ground must be checked')
    allowed=('ttl_3v3','rs232') if 'qiming' in fixture['board_id'] else ('ttl_3v3',)
    require(serial['electrical_mode'] in allowed,'serial electrical interface mismatch')
    require(isinstance(serial['path'],str) and serial['path'],'observed serial device required')
    if model:
        serial_key=hashlib.sha256(serial['path'].encode()).hexdigest()
        serial_device={'kind':'model','supplied_path':serial['path']}
    else:
        serial_path=Path(serial['path']).resolve(strict=True)
        observed=serial_path.stat()
        require(stat.S_ISCHR(observed.st_mode),'serial path must be an actual character device')
        require(re.fullmatch(r'tty[A-Za-z0-9_.-]+',serial_path.name) is not None,
                'serial path is not an identified tty device')
        # This lease directory belongs to one host/kernel. Paths are audit
        # data: different nodes/hardlinks can name the same character device.
        serial_key=hashlib.sha256(f'local-character-device:{observed.st_rdev}'.encode()).hexdigest()
        serial_device={'kind':'character-device','lease_scope':'local-host',
            'supplied_path':serial['path'],'canonical_path':str(serial_path),
            'st_rdev':observed.st_rdev}
    require(isinstance(station['adapter_files'],list) and station['adapter_files'],'reviewed adapter file identities required')
    for identity in station['adapter_files']:
        verify_file_identity(identity)
    identities+=station['adapter_files']
    # Every actual command interpreter/executable is explicitly pinned.
    pinned={str(verify_file_identity(identity).resolve()) for identity in identities}
    for argv in station['commands'].values():
        expanded=command(argv,{'firmware':'firmware.bin',**station['board']})
        executable=regular_file(expanded[0]).resolve()
        require(str(executable) in pinned,'command executable is not an identified reviewed tool')
    return identities,'serial-'+serial_key,serial_device

def run(fixture_path,station_path,build_dir,image,static_report,report_path,leases,
        *,execute_hardware=False,model=False,artifact_validator=None,adapter_runner=None):
    report={'schema_version':1,'kind':'board_fixture_model' if model else 'board_fixture_preflight',
            'hardware_verified':False,'eligible_physical_hil':False,'status':'fail','operations':[],
            'started_utc':utc_now()}
    protected=[fixture_path,station_path,static_report]
    safe=all(report_path.resolve()!=path.resolve() for path in protected)
    try:
        if (artifact_validator or adapter_runner) and not model:
            raise EvidenceError('dependency injection requires explicit model mode')
        require(safe,'report must not overwrite fixture/station/static evidence')
        fixture=validate_fixture(load_json(fixture_path))
        station=load_json(station_path)
        reject_secret_fields(station)
        build=(artifact_validator or validate_build)(fixture,build_dir,image,static_report)
        inputs,serial_lease,serial_device=validate_station(station,fixture,model=model)
        artifacts=build['artifacts']+[file_identity(fixture_path),file_identity(station_path)]
        protected.extend(Path(item['path']) for item in artifacts+inputs)
        safe=all(report_path.resolve()!=path.resolve() for path in protected)
        require(safe,'report must not overwrite a pinned input')
        manifest={'schema_version':1,'kind':'orchestrator_model' if model else 'physical_hil',
            'lab_id':station['lab_id'],'board':station['board'],'firmware':build['firmware'],
            'config_sha256':build['config_sha256'],'adapter_files':inputs+artifacts,
            'commands':station['commands'],'operation_timeout_s':station['operation_timeout_s'],
            'total_timeout_s':station['total_timeout_s'],'required_tests':fixture['required_tests'],
            'budgets':station['budgets'],'serial_lease_id':serial_lease}
        validate_hil(manifest,model=model)
        report.update({'status':'ready','dry_run':not execute_hardware and not model,
            'fixture_sha256':digest(fixture_path),'station_sha256':digest(station_path),
            'build_identity':build,'board':station['board'],'planned_operations':list(station['commands']),
            'serial_device':serial_device,'pending_qualification':fixture['pending_qualification']})
        if execute_hardware or model:
            import tempfile
            with tempfile.TemporaryDirectory() as temporary:
                manifest_path=Path(temporary)/'admitted.json'
                atomic_json(manifest_path,manifest)
                execution=execute(manifest_path,report_path.with_suffix('.execution.json'),leases,
                    model=model,execute_hardware=execute_hardware,adapter_runner=adapter_runner)
            for identity in artifacts+inputs: verify_file_identity(identity)
            report.update({'kind':'board_fixture_model' if model else 'physical_hil',
                'status':execution['status'],'hardware_verified':execution['hardware_verified'],
                'eligible_physical_hil':execution['eligible_physical_hil'],'operations':execution['operations'],
                'execution':execution,'config_sha256':build['config_sha256'],
                'firmware_sha256':build['firmware']['sha256']})
            for key in ('tests','metrics','transcript','failure','lease_status'):
                if key in execution: report[key]=execution[key]
    except (EvidenceError,OSError,ValueError,KeyError,TypeError) as error:
        report.update(status='fail',failure=str(error),hardware_verified=False,eligible_physical_hil=False)
    report['finished_utc']=utc_now()
    if safe: atomic_json(report_path,report)
    return report

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture',type=Path,required=True)
    parser.add_argument('--station',type=Path,required=True)
    parser.add_argument('--build-dir',type=Path,required=True)
    parser.add_argument('--image',default='uart_echo')
    parser.add_argument('--static-report',type=Path,required=True)
    parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--leases',type=Path,required=True)
    parser.add_argument('--execute',action='store_true')
    args=parser.parse_args()
    result=run(args.fixture,args.station,args.build_dir,args.image,args.static_report,
               args.report,args.leases,execute_hardware=args.execute)
    print(f"Board fixture: {result['status']}; hardware_verified={result['hardware_verified']}")
    return 0 if result['status'] in ('ready','pass') else 1
if __name__=='__main__': raise SystemExit(main())
