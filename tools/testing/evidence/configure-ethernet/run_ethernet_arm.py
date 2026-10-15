from pathlib import Path
import subprocess,os,json,hashlib
repo=Path('/workspace/nexus');out=Path('/workspace/nexus-config-iteration-evidence/ethernet-arm-first');out.mkdir(exist_ok=True)
env=os.environ.copy();env['PATH']='/workspace/nexus/.venv/bin:/workspace/nexus-tools-arm/arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi/bin:'+env['PATH'];records=[];artifacts=[];failures=[];directory=repo/'tests/contracts/ethernet_assembly'
def run(key,index,command):
 result=subprocess.run(command,cwd=repo,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);log=out/f'{key}-{index}.log';log.write_bytes(result.stdout);records.append({'command':command,'cwd':str(repo),'PATH':env['PATH'],'exit_code':result.returncode,'log':str(log),'sha256':hashlib.sha256(result.stdout).hexdigest()});return result
for label in ('stm32-zg','gd32'):
 for backend in ('baremetal','freertos'):
  key=f'{label}-{backend}';assembly=out/(key+'.toml');content=(directory/(label+'.toml')).read_text().replace('backend="baremetal"',f'backend="{backend}"').replace(f'board_package="{label}-board"',f'board_package="{directory}/{label}-board"');assembly.write_text(content);build=out/key
  commands=[['cmake','-S',str(directory),'-B',str(build),'-G','Ninja',f'-DCMAKE_TOOLCHAIN_FILE={repo}/cmake/toolchains/arm-gcc.cmake',f'-DNEXUS_SOURCE_ROOT={repo}',f'-DNEXUS_ASSEMBLY_FILE={assembly}','-DCMAKE_BUILD_TYPE=Release'],['cmake','--build',str(build),'--target','ethernet_controller_fixture','-j','4']]
  for index,command in enumerate(commands):
   result=run(key,index,command)
   if result.returncode:failures.append(key);print(key,result.stdout.decode()[-1800:],flush=True);break
  else:
   elf=next((build/'bin').glob('*.elf'));r=run(key,'size',['arm-none-eabi-size',str(elf)]);n=run(key,'nm',['arm-none-eabi-nm','-S','--size-sort',str(elf)])
   for p in [*(build/'bin').iterdir(),*(build/'nexus-sdk/generated').iterdir(),build/'compile_commands.json']:
    if p.is_file():artifacts.append({'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'size':p.stat().st_size})
   if r.returncode or n.returncode:failures.append(key)
   print(key,'PASS',r.stdout.decode().strip().splitlines()[-1],flush=True)
(out/'development.json').write_text(json.dumps({'scope':'Actual four selected Ethernet production graphs; compile/link budgets and ETH/MDIO ops retention only. No PHY/refclock/wiring/HIL or network/product qualification.','commands':records,'artifacts':artifacts,'failures':failures},indent=2)+'\n');raise SystemExit(bool(failures))
