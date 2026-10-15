from pathlib import Path
import json,hashlib,re,shlex,subprocess,shutil
r=Path('/workspace/nexus');s=Path(__file__).parent;ev=r/'tools/testing/evidence/clock-domain-iteration/integration';results=[]
def ident(p):
 p=Path(p);return {'path':str(p.relative_to(r)) if p.is_relative_to(r) else str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'size':p.stat().st_size}
for family in ['stm32','gd32']:
 for selected in [False,True]:
  label=family+('-selected' if selected else '-default')+'-final2';build=s/('build-'+label);out=ev/label
  graph=json.loads((out/'compile_commands.json').read_text());boundary=json.loads((ev/(label+'-source-boundary.json')).read_text());assert boundary['unchanged']
  symbols=(ev/(label+'-symbols.log')).read_text();asm=(ev/(label+'-disassembly.log')).read_text();resources=json.loads((out/'sleep_consumer.resources.json').read_text());assert resources['status']=='passed'
  expected_symbol='nx_stm32f407_sleep' if family=='stm32' else 'nx_gd32f470_sleep'
  for symbol in [expected_symbol,'nx_clock_domain_construct','nx_clock_domain_close','nx_clock_domain_quiescent','nx_arch_wait_for_interrupt']:
   assert bool(re.search(r'\b'+symbol+r'$',symbols,re.M))==selected,(label,symbol)
  assert bool(re.search(r'\bwfi\b',asm))==selected,label
  family_sources=[x['file'] for x in graph if x['file'].endswith('/soc/'+('stm32f407' if family=='stm32' else 'gd32f470')+'/sleep.c')]
  assert bool(family_sources)==selected
  built_domain=list(build.glob('**/clock_domain.c.obj'));built_sleep=list(build.glob('**/sleep.c.obj'));archives=list(build.glob('**/*clock_domain*.a'));assert bool(built_domain)==selected and bool(built_sleep)==selected and bool(archives)==selected
  dependencies=set()
  for dep in build.rglob('*.obj.d'):
   text=dep.read_text().replace('\\\n',' ');names=shlex.split(text.split(':',1)[1])
   for name in names:
    f=Path(name)
    if f.is_file():dependencies.add(f)
  (out/'compiled-dependencies.json').write_text(json.dumps([ident(x) for x in sorted(dependencies)],indent=2)+'\n')
  for name in ['nexus_factory.h','nexus_factory_strong.h','selection.cmake']:
   src=build/'nexus-sdk/generated'/name
   if src.is_file():shutil.copyfile(src,out/'generated'/name)
  results.append({'label':label,'selected':selected,'resources_status':resources['status'],'flash_load_bytes':resources['metrics']['flash']['loaded_bytes'],'sram_including_msp_bytes':resources['metrics']['ram']['sram']['reserved_bytes'],'main_stack_bytes':resources['metrics']['reservations']['msp']['bytes'],'family_sleep_export_entry':family_sources,'domain_registered_export_entry':any(x['file'].endswith('/components/clock_domain/src/clock_domain.c') for x in graph),'domain_object_generated':bool(built_domain),'family_sleep_object_generated':bool(built_sleep),'domain_archive_generated':bool(archives),'expected_final_symbols_present':selected,'wfi_instruction_present':selected,'compile_source_boundary_unchanged':True,'firmware_files':[ident(out/n) for n in ['sleep_consumer.elf','sleep_consumer.bin','sleep_consumer.map','sleep_consumer.resources.json']],'nominal_board_electrical_and_hil':'not_executed'})
for f in ['stm32','gd32']:
 default,chosen=[x for x in results if x['label'].startswith(f)]
 chosen['delta_flash_load_bytes']=chosen['flash_load_bytes']-default['flash_load_bytes'];chosen['delta_sram_including_msp_bytes']=chosen['sram_including_msp_bytes']-default['sram_including_msp_bytes']
(ev/'graph-elf-checks.json').write_text(json.dumps({'status':'passed','builds':results,'checks':'assertions executed against actual compiler export, generated object/archive existence, exact ELF nm/disassembly/resource reports'},indent=2)+'\n')
print([(x['label'],x['flash_load_bytes'],x['sram_including_msp_bytes']) for x in results])
