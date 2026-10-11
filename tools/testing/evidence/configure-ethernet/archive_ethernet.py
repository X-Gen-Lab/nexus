from pathlib import Path
import json,hashlib
repo=Path('/workspace/nexus');origin=Path('/workspace/nexus-config-iteration-evidence');target=repo/'tools/testing/evidence/configure-ethernet';target.mkdir(parents=True,exist_ok=False)
artifacts=[];seen=set()
def retain(source,label=None):
 source=Path(source)
 if not source.is_file() or str(source) in seen:return
 seen.add(str(source));name=Path(label or source.relative_to(origin))
 if name.suffix in {'.c','.h','.cpp','.hpp'}:name=Path(str(name)+'.raw')
 dest=target/name;dest.parent.mkdir(parents=True,exist_ok=True);data=source.read_bytes();dest.write_bytes(data)
 artifacts.append({'original_path':str(source),'path':dest.relative_to(repo).as_posix(),'size':len(data),'sha256':hashlib.sha256(data).hexdigest()})
for pattern in ('ethernet-*.log','ethernet-*.xml','ethernet-*.command.json'):
 for p in sorted(origin.glob(pattern)):retain(p)
compiled=set()
for stem in ('ethernet-arm-first','ethernet-arm-green'):
 base=origin/stem
 for p in sorted(base.iterdir()):
  if p.is_file():retain(p)
 for build in sorted(p for p in base.iterdir() if p.is_dir()):
  for directory in (build/'bin',build/'generated',build/'nexus-sdk/generated'):
   if directory.exists():
    for p in sorted(directory.iterdir()):
     if p.is_file():retain(p)
  cc=build/'compile_commands.json';retain(cc)
  if cc.is_file():
   for entry in json.loads(cc.read_text()):
    p=Path(entry['file'])
    if not p.is_absolute():p=Path(entry['directory'])/p
    if p.is_file():compiled.add(p.resolve())
for build in (repo/'build/ethernet-config-model',repo/'build/ethernet-config-san'):
 retain(build/'compile_commands.json','host/'+build.name+'/compile_commands.json')
 for family in ('stm32','gd32'):
  directory=build/'tests/contracts/google'/('eth-factory-'+family)
  for p in sorted(directory.iterdir()):
   if p.is_file():retain(p,'host/'+build.name+'/'+family+'/'+p.name)
 cc=build/'compile_commands.json'
 if cc.is_file():
  for entry in json.loads(cc.read_text()):
   p=Path(entry['file'])
   if not p.is_absolute():p=Path(entry['directory'])/p
   if p.is_file():compiled.add(p.resolve())
for p in sorted(compiled):
 if p.is_relative_to(repo):retain(p,'captured-compiled-sources/'+p.relative_to(repo).as_posix())
 elif p.is_relative_to(origin):retain(p)
owned=['tools/configure/'+name for name in ('ethernet.py','test_ethernet.py','test_ethernet_memory.py','configure.py','authored.py','assembly.schema.json','ir.py','factory.py','bindings.py','providers/common.py')]
for family in ('stm32f407','gd32f470'):
 owned += ['tools/configure/providers/'+family+'/'+name for name in ('__init__.py','bindings.py','emission.py')]
 owned += ['soc/'+family+'/'+name for name in ('soc.json','routes.json')]
owned += ['tests/google/'+name for name in ('ethernet_factory_test.cpp','ethernet_factory_model.h','ethernet_factory.cmake')]
owned += [p.relative_to(repo).as_posix() for p in sorted((repo/'tests/contracts/ethernet_assembly').rglob('*')) if p.is_file() and '__pycache__' not in p.parts]
sources=[]
for name in owned:
 p=repo/name;data=p.read_bytes();sources.append({'path':name,'size':len(data),'sha256':hashlib.sha256(data).hexdigest()});retain(p,'frozen-owned-sources/'+name)
review=Path('/workspace/nexus-industrial-design/reviews/ethernet-assembly-independent.json');retain(review,'independent-read-only-review.json')
for name in ('run_ethernet_arm.py','run_ethernet_arm_green.py','run_eth_cmd.py','archive_ethernet.py'):retain(origin/name)
records=[]
for p in sorted(origin.glob('ethernet-*.command.json')):records.append({'original_metadata':str(p),'record':json.loads(p.read_text())})
for stem in ('ethernet-arm-first','ethernet-arm-green'):
 p=origin/stem/'development.json';records.append({'original_metadata':str(p),'record':json.loads(p.read_text())})
mapping={item['original_path']:item['path'] for item in artifacts}
def stored(value):
 if isinstance(value,list):return [stored(item) for item in value]
 if not isinstance(value,dict):return value
 result={}
 for key,item in value.items():
  if key in {'path','log','original_metadata'} and isinstance(item,str) and item in mapping:
   result['original_'+key if not key.startswith('original_') else key]=item;result[key if not key.startswith('original_') else 'metadata_path']=mapping[item]
  else:result[key]=stored(item)
 return result
manifest={'scope':'Dirty-development proof only: genuine selected Ethernet configuration RED/GREEN, public MDIO binding header RED/GREEN, ELF memory observer Python RED/GREEN, production generated factory+common MAC+actual family SDK leaves RED 2 cold register refusals per family -> GREEN 15 per family; fresh ASan/UBSan 15 per family; four actual ST F407ZG/GD F470ZG × baremetal/FreeRTOS ARM links PASS resource budget, actual ops retention and exact actual SRAM domain/count/LOAD observer. No PHY presence/address/reset/reference-clock, connector/magnetics/electrical/network/HIL or clean-source candidate/release qualification.','nonqualifying_attempts':'Initial factory compile had test spelling errors and an actual SDK unsigned-long mask conversion warning. Initial first getter run crashed in test teardown polling an uninitialized context (no XML); retained, not qualifying. Red3 included a GMock expectation setup error. Red4 is the qualifying actual 11-case execution, exactly two cold refusal failures per family. Failed original ARM links prove actual missing MDIO declaration dependency; later four fresh links all pass.','review':'Independent read-only cold lifecycle review reported no new blocker and records exact nine source identities. It did not rerun any model or ARM execution.','memory_boundary':'Host addresses substitute only the external nx_dma_buffer_validate boundary: exact approved domain, alignment, direction and object span are checked. Real ARM ELF post-observer separately proves physical SRAM allocation, descriptors, RX capacities, non-overlap, readonly domain bytes and writable LOAD coverage.','default_zero_cost':'Two fresh default ARM images recorded in configure-usb-corrective archive contain no selected ETH/MDIO runtime storage, table, methods or strong IRQ. This is a separate original scope, not repeated hardware evidence.','sources':sources,'commands':stored(records),'artifacts':artifacts}
(target/'development.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
for item in artifacts:assert hashlib.sha256((repo/item['path']).read_bytes()).hexdigest()==item['sha256']
print('ETH final freeze',len(sources),'sources',len(artifacts),'raw artifacts; manifest',hashlib.sha256((target/'development.json').read_bytes()).hexdigest())
