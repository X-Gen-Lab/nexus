from pathlib import Path
import json,hashlib
repo=Path('/workspace/nexus'); origin=Path('/workspace/nexus-config-iteration-evidence'); snapshot=Path('/workspace/nexus-usb-corrective-config'); target=repo/'tools/testing/evidence/configure-usb-corrective'; target.mkdir(parents=True,exist_ok=False)
artifacts=[]; seen=set()
def retain(source,label=None):
 source=Path(source)
 if not source.is_file() or str(source) in seen:return
 seen.add(str(source)); name=Path(label or source.relative_to(origin))
 if name.suffix in {'.c','.h','.cpp','.hpp'}: name=Path(str(name)+'.raw')
 dest=target/name; dest.parent.mkdir(parents=True,exist_ok=True); data=source.read_bytes(); dest.write_bytes(data)
 artifacts.append({'original_path':str(source),'path':dest.relative_to(repo).as_posix(),'size':len(data),'sha256':hashlib.sha256(data).hexdigest()})
for pattern in ('usb-lifetime-*','usb-gate-*','usb-corrective-*'):
 for p in sorted(origin.glob(pattern)):retain(p)
compiled=set()
for stem in ('usb-arm-corrective-green','usb-default-corrective-green'):
 base=origin/stem
 for p in sorted(base.iterdir()):
  if p.is_file():retain(p)
 for build in sorted(p for p in base.iterdir() if p.is_dir()):
  for directory in (build/'bin',build/'generated',build/'nexus-sdk/generated'):
   if directory.exists():
    for p in sorted(directory.iterdir()):
     if p.is_file():retain(p)
  compile_file=build/'compile_commands.json'; retain(compile_file)
  if compile_file.is_file():
   for entry in json.loads(compile_file.read_text()):
    p=Path(entry['file'])
    if not p.is_absolute():p=Path(entry['directory'])/p
    if p.is_file():compiled.add(p.resolve())
for p in sorted(compiled):
 if p.is_relative_to(repo):retain(p,'captured-compiled-sources/'+p.relative_to(repo).as_posix())
 elif p.is_relative_to(origin):retain(p)
owned=json.loads((snapshot/'snapshot.json').read_text()); sources=[]
for item in owned:
 name=item['path']; p=repo/name; data=p.read_bytes(); sources.append({'path':name,'size':len(data),'sha256':hashlib.sha256(data).hexdigest(),'scope':'Actual shared development source at capture; binding/ST emission contain unselected in-progress ETH support.'})
 retain(snapshot/name,'isolated-corrective-sources/'+name)
retain(snapshot/'snapshot.json','isolated-corrective-snapshot.json');retain(snapshot/'corrective.patch','isolated-corrective.patch')
retain(origin/'run_usb_arm_corrective.py');retain(origin/'run_usb_default_corrective.py');retain(origin/'archive_usb_corrective.py')
records=[]
for pattern in ('usb-lifetime-*.command.json','usb-gate-*.command.json','usb-corrective-*.command.json'):
 for p in sorted(origin.glob(pattern)):records.append({'original_metadata':str(p),'record':json.loads(p.read_text())})
for stem in ('usb-arm-corrective-green','usb-default-corrective-green'):
 p=origin/stem/'development.json';records.append({'original_metadata':str(p),'record':json.loads(p.read_text())})
mapping={item['original_path']:item['path'] for item in artifacts}
def stored(value):
 if isinstance(value,list):return [stored(item) for item in value]
 if not isinstance(value,dict):return value
 result={}
 for key,item in value.items():
  if key in {'path','log','original_metadata'} and isinstance(item,str) and item in mapping:
   result['original_'+key if not key.startswith('original_') else key]=item; result[key if not key.startswith('original_') else 'metadata_path']=mapping[item]
  else:result[key]=stored(item)
 return result
manifest={'scope':'Dirty-development corrective proof only: actual lifetime epoch ABA/exhaustion RED, actual STM GPIO refusal RED, normal-target-scoped hook production GMock GREEN 23/23, six actual corrective selected ARM graphs budgets/retention PASS, two fresh default ARM graphs with zero selected USB/ETH/MDIO runtime. Original USB 30-source/436-raw freeze remains unchanged. Neither source-clean candidate nor physical connector/VBUS/electrical/enumeration/class/HIL qualification.','failed_attempt':'usb-gate-red compile attempts failed before binary execution because a temporary forced stdint header changed POSIX feature visibility. They are preserved but do not count as RED qualification. usb-gate-real-red proves the actual GPIO failure after the test-only builtin register type corrected that boundary.','source_note':'Isolated corrective five-file snapshot derives from original frozen USB source and excludes ETH; actual development ELF graphs used current shared source with unselected ETH changes. Source scopes are explicitly distinct; parent independent normal hooks on isolated closed source establish its own evidence.','review':'Independent read-only USB lifecycle review closed both reported blockers after inspecting lifetime epoch preservation and verified owned GPIO withdrawal; reviewer did not claim to rerun these raw model/ARM commands.','sources':sources,'isolated_corrective_sources':owned,'commands':stored(records),'artifacts':artifacts}
(target/'development.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
for a in artifacts:
 p=repo/a['path'];assert hashlib.sha256(p.read_bytes()).hexdigest()==a['sha256']
print(len(artifacts),'immutable raw artifacts',len(sources),'actual source identities; manifest',hashlib.sha256((target/'development.json').read_bytes()).hexdigest())
