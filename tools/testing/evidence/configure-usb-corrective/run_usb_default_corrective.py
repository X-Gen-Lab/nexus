from pathlib import Path
import subprocess,os,json,hashlib
repo=Path('/workspace/nexus');out=Path('/workspace/nexus-config-iteration-evidence/usb-default-corrective-green');out.mkdir(exist_ok=True)
env=os.environ.copy();env['PATH']='/workspace/nexus/.venv/bin:/workspace/nexus-tools-arm/arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi/bin:'+env['PATH'];records=[];artifacts=[];failures=[]
for label,assembly in [('stm32-zg','qiming-baremetal'),('gd32','liangshan-baremetal')]:
 build=out/label
 commands=[['cmake','-S',str(repo),'-B',str(build),'-G','Ninja',f'-DCMAKE_TOOLCHAIN_FILE={repo}/cmake/toolchains/arm-gcc.cmake',f'-DNEXUS_ASSEMBLY_FILE={repo}/tools/configure/assemblies/{assembly}.toml','-DNEXUS_BUILD_TESTS=OFF','-DCMAKE_BUILD_TYPE=Release'],['cmake','--build',str(build),'--target','nexus_firmware','-j','4'],['arm-none-eabi-nm','-S','--size-sort',str(build/'bin/nexus_firmware.elf')],['arm-none-eabi-size',str(build/'bin/nexus_firmware.elf')]]
 for index,command in enumerate(commands):
  r=subprocess.run(command,cwd=repo,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);log=out/f'{label}-{index}.log';log.write_bytes(r.stdout);records.append({'command':command,'cwd':str(repo),'PATH':env['PATH'],'exit_code':r.returncode,'log':str(log),'sha256':hashlib.sha256(r.stdout).hexdigest()})
  if r.returncode:failures.append(label);print(label,r.stdout.decode()[-2000:],flush=True);break
  if index==2:
   symbols=r.stdout.decode();forbidden=[line for line in symbols.splitlines() if ('nx_usb' in line.lower() or 's_nx_usb' in line.lower() or 'nx_eth_' in line.lower() or 's_nx_eth_' in line.lower() or 'nx_mdio_' in line.lower() or (any(token in line.lower() for token in ('otg_fs','usbfs','eth_irqhandler','enet_irqhandler')) and line.split()[-2] != 'W'))]
   if forbidden:failures.append(label);print('unexpected USB runtime symbols',forbidden,flush=True);break
 else:
  for p in [*(build/'bin').iterdir(),*(build/'generated').iterdir(),build/'compile_commands.json']:
   if p.is_file():artifacts.append({'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'size':p.stat().st_size})
  print(label,'DEFAULT GC PASS',flush=True)
(out/'development.json').write_text(json.dumps({'scope':'Actual default production ARM graph without USB selection; default ELF has no USB/ETH/MDIO runtime storage, tables, methods or strong IRQs; official weak startup aliases remain. Compile/link only, no hardware or product qualification.','commands':records,'artifacts':artifacts,'failures':failures},indent=2)+'\n');raise SystemExit(bool(failures))
