from pathlib import Path
import json,os,subprocess,hashlib,shutil
r=Path('/workspace/nexus');s=Path(__file__).parent;ev=r/'tools/testing/evidence/clock-domain-iteration/integration';fixture=ev/'fixture';fixture.mkdir(exist_ok=True)
for name in ['CMakeLists.txt','main.c','stm32.toml','gd32.toml']:
 shutil.copyfile(s/name,fixture/name)
env=os.environ.copy();env['PATH']='/workspace/nexus-tools-arm/arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi/bin:'+env['PATH'];records=[]
for family in ['stm32','gd32']:
 for selected in [False,True]:
  label=family+('-selected' if selected else '-default')+'-final2';build=s/('build-'+label)
  commands=[[str(r/'.venv/bin/cmake'),'-S',str(s),'-B',str(build),'-DCMAKE_TOOLCHAIN_FILE='+str(r/'cmake/toolchains/arm-gcc.cmake'),'-DNEXUS_ASSEMBLY_FILE='+str(s/(family+'.toml')),'-DNEXUS_SLEEP_SOURCE='+str(r),'-DSLEEP_SELECTED='+('ON' if selected else 'OFF')],[str(r/'.venv/bin/cmake'),'--build',str(build),'--target','sleep_consumer','--parallel','2']]
  before={}
  for i,cmd in enumerate(commands):
   if i==1:
    graph=json.loads((build/'compile_commands.json').read_text());before={x['file']:hashlib.sha256(Path(x['file']).read_bytes()).hexdigest() for x in graph}
   p=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True);log=label+'-'+str(i)+'.log';(ev/log).write_text(p.stdout);records.append({'command':cmd,'exit_code':p.returncode,'log':log});print(log,p.returncode,p.stdout[-1500:],flush=True)
   if p.returncode:break
  if p.returncode:continue
  after={x:hashlib.sha256(Path(x).read_bytes()).hexdigest() for x in before}
  (ev/(label+'-source-boundary.json')).write_text(json.dumps({'before':before,'after':after,'unchanged':before==after},indent=2)+'\n')
  out=ev/label;out.mkdir(exist_ok=True)
  shutil.copyfile(build/'compile_commands.json',out/'compile_commands.json')
  for f in (build/'bin').iterdir():
   if f.is_file():shutil.copyfile(f,out/f.name)
  generated=out/'generated';generated.mkdir(exist_ok=True)
  for name in ['resolved.json','nexus_config.h','resource-budget.json','memory.ld','input_paths.json','input_hashes.json','bindings.c','nexus_bindings.h']:
   src=build/'nexus-sdk/generated'/name
   if src.exists():shutil.copyfile(src,generated/name)
(ev/'final2-matrix-commands.json').write_text(json.dumps(records,indent=2)+'\n')
