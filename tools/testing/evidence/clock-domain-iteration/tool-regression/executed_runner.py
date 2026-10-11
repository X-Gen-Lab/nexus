"""Execute real maintained unittest cases against old/new CMake source graphs."""
from pathlib import Path
import hashlib,importlib.util,io,json,os,shutil,subprocess,sys,tempfile,time,unittest
import xml.etree.ElementTree as ET
REPO=Path('/workspace/nexus')
EVIDENCE=REPO/'tools/testing/evidence/clock-domain-iteration/tool-regression'
EVIDENCE.mkdir(exist_ok=True)
phase=sys.argv[1]
assert phase in ('red','green')
spec=importlib.util.spec_from_file_location('sleep_selection',REPO/'tools/testing/test_soc_sleep_selection.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
def identity(path):
 return {'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'size':path.stat().st_size}
with tempfile.TemporaryDirectory(prefix='nexus-pre-fix-soc-') as temporary:
 mirror=Path(temporary)
 if phase=='red':
  for directory in ('vendors','scripts','components','.venv'):
   (mirror/directory).symlink_to(REPO/directory,target_is_directory=True)
  (mirror/'cmake/platform').mkdir(parents=True)
  shutil.copyfile(REPO/'cmake/platform/ControllerModes.cmake',mirror/'cmake/platform/ControllerModes.cmake')
  archive=EVIDENCE/'pre-fix-soc';archive.mkdir(exist_ok=True)
  for family in ('stm32f407','gd32f470'):
   source=REPO/'soc'/family/'CMakeLists.txt';target=mirror/'soc'/family;target.mkdir(parents=True)
   text=source.read_text()
   anchor='    get_filename_component(sleep_source\n        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)\n'
   assert text.count(anchor)==1
   text=text.replace(anchor,'').replace('${sleep_source}','${NEXUS_SOURCE_DIR}')
   (target/'CMakeLists.txt').write_text(text)
   shutil.copyfile(target/'CMakeLists.txt',archive/(family+'-CMakeLists.txt'))
   shutil.copyfile(REPO/'soc'/family/'sleep.c',target/'sleep.c')
   (target/'private').symlink_to(REPO/'soc'/family/'private',target_is_directory=True)
   if family=='gd32f470':shutil.copyfile(REPO/'soc'/family/'timebase.c',target/'timebase.c')
  module.ROOT=mirror
 records=[]
 real_run=subprocess.run
 def capture(*args,**kwargs):
  start=time.monotonic();process=real_run(*args,**kwargs);index=len(records)
  stem=phase+'-cmake-'+str(index);command=list(args[0])
  (EVIDENCE/(stem+'-stdout.log')).write_text(process.stdout)
  (EVIDENCE/(stem+'-stderr.log')).write_text(process.stderr)
  fixture=Path(command[command.index('-S')+1]);copied=EVIDENCE/(stem+'-fixture');copied.mkdir(exist_ok=True)
  for name in ('CMakeLists.txt','sdk-port/CMakeLists.txt','generated/resolved.json','build/properties.txt'):
   src=fixture/name
   if src.is_file():
    dest=copied/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(src,dest)
  records.append({'command':command,'cwd':str(Path.cwd()),'exit_code':process.returncode,'elapsed_seconds':time.monotonic()-start,'stdout':stem+'-stdout.log','stderr':stem+'-stderr.log','captured_fixture':str(copied.relative_to(EVIDENCE))})
  return process
 module.subprocess.run=capture
 class RecordedResult(unittest.TextTestResult):
  def __init__(self,*args,**kwargs):super().__init__(*args,**kwargs);self.cases=[]
  def startTest(self,test):
   super().startTest(test);self.item={'id':test.id(),'start':time.monotonic()};self.cases.append(self.item)
  def stopTest(self,test):self.item['seconds']=time.monotonic()-self.item['start'];super().stopTest(test)
  def addFailure(self,test,err):self.item['failure']=self._exc_info_to_string(err,test);super().addFailure(test,err)
  def addError(self,test,err):self.item['error']=self._exc_info_to_string(err,test);super().addError(test,err)
  def addSkip(self,test,reason):self.item['skip']=reason;super().addSkip(test,reason)
 stream=io.StringIO();result=unittest.TextTestRunner(stream=stream,verbosity=2,resultclass=RecordedResult).run(unittest.defaultTestLoader.loadTestsFromTestCase(module.SocSleepSelectionTests))
 subprocess.run=real_run
 log=stream.getvalue();(EVIDENCE/(phase+'-unittest.log')).write_text(log)
 suite=ET.Element('testsuite',name='soc_sleep_selection',tests=str(result.testsRun),failures=str(len(result.failures)),errors=str(len(result.errors)),skipped=str(len(result.skipped)))
 for item in result.cases:
  case=ET.SubElement(suite,'testcase',name=item['id'],time=str(item['seconds']))
  for kind in ('failure','error','skip'):
   if kind in item:ET.SubElement(case,'skipped' if kind=='skip' else kind).text=item[kind]
 ET.ElementTree(suite).write(EVIDENCE/(phase+'.xml'),encoding='utf-8',xml_declaration=True)
 (EVIDENCE/(phase+'-commands.json')).write_text(json.dumps(records,indent=2)+'\n')
 source=[identity(REPO/'tools/testing/test_soc_sleep_selection.py')]+[identity(REPO/'soc'/family/'CMakeLists.txt') for family in ('stm32f407','gd32f470')]
 (EVIDENCE/(phase+'-result.json')).write_text(json.dumps({'command':[sys.executable,str(Path(__file__).resolve()),phase],'cases':result.testsRun,'failures':len(result.failures),'errors':len(result.errors),'skipped':len(result.skipped),'source':source,'pre_fix_only':'Temporary two actual SoC CMake source copies revert only function-private source anchor to original caller-variable paths; SDK/scripts remain symlinked to pinned maintained inputs.' if phase=='red' else None},indent=2)+'\n')
 print(log)
 if phase=='red':sys.exit(1 if len(result.failures)==2 and not result.errors and not result.skipped and result.testsRun==4 else 3)
 sys.exit(0 if result.wasSuccessful() and not result.skipped and result.testsRun==4 else 1)
