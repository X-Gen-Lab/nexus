"""Production HIL admission/orchestration with explicit nonphysical adapters."""
from pathlib import Path
import copy
import hashlib
import json
import sys
import tempfile
import types
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from scripts.evidence.common import EvidenceError,atomic_json,digest,file_identity
from scripts.hil.board_fixture import ROOT,run,validate_fixture,validate_station
from scripts.hil.run_hil import EquipmentLease,execute
from scripts.hil.serial_echo import observe

class BoardFixtureModels(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory()
        self.root=Path(self.temporary.name)
        self.fixture=json.loads((ROOT/'scripts/hil/fixtures/stm32f407_qiming_v31.json').read_text())
        self.fixture_path=self.root/'fixture.json'
        atomic_json(self.fixture_path,self.fixture)
        self.image=self.root/'uart_echo.bin'; self.image.write_bytes(b'nonphysical binary model')
        self.static=self.root/'static.json'; atomic_json(self.static,{'kind':'model artifact fixture'})
        self.config=self.root/'config'; self.config.write_bytes(b'model configuration')
        self.adapter=self.root/'adapter.py'; self.adapter.write_text('# reviewed nonphysical adapter\n')
        self.tool=Path(sys.executable).resolve()
        self.station={'schema':1,'kind':'fixture_model','lab_id':'model-station',
            'board':{'id':'inventory-model-1','profile':self.fixture['board_id'],'revision':'model-A',
                'probe_serial':'model-probe-1','chip_part':self.fixture['chip_part'],'chip_uid':'model-001'},
            'probe':{'backend':'external-reviewed','interface':'external-reviewed','transport':'swd',
                'tool':file_identity(self.tool),'interface_config':None,'target_config':None},
            'serial':{'path':'model:serial-1','baudrate':115200,'electrical_mode':'ttl_3v3',
                'wiring_checked':True,'ground_connected':True},
            'power':{'voltage_mv':3300,'source':'model-power'},'outputs_disconnected':True,
            'adapter_files':[file_identity(self.adapter)],
            'commands':{name:[str(self.tool),str(self.adapter),name] for name in
                ('identify','flash','verify_flash','reset','serial','cleanup')},
            'operation_timeout_s':2,'total_timeout_s':10,'budgets':{}}
        self.station_path=self.root/'station.json'
        self.report=self.root/'result.json'; self.leases=self.root/'leases'
        self.seen=[]; self.mode=None
    def tearDown(self): self.temporary.cleanup()
    def artifacts(self,*_):
        return {'board_sha256':'a'*64,'layout_sha256':'b'*64,'config_sha256':digest(self.config),
            'firmware':{'path':str(self.image),'sha256':digest(self.image)},
            'elf_sha256':'c'*64,'load_address':0x08000000,'image_size':self.image.stat().st_size,
            'artifacts':[file_identity(self.image),file_identity(self.config),file_identity(self.static)]}
    def adapter_runner(self,argv,timeout):
        name=argv[-1]; self.seen.append(name)
        board=copy.deepcopy(self.station['board'])
        if name=='identify':
            if self.mode=='wrong_uid': board['chip_uid']='different-model-device'
            return json.dumps({'schema_version':1,'board':board}).encode()
        if name=='flash' and self.mode=='changed_image': self.image.write_bytes(b'changed model bytes')
        if name=='verify_flash':
            sha='f'*64 if self.mode=='bad_readback' else digest(self.image)
            return json.dumps({'schema_version':1,'board':board,'firmware_sha256':sha}).encode()
        if name=='serial':
            tests=[{'id':item,'status':'pass'} for item in self.fixture['required_tests']]
            if self.mode=='zero_tests': tests=[]
            if self.mode=='skipped_test': tests[0]['status']='skipped'
            return json.dumps({'schema_version':1,'board':board,'firmware_sha256':digest(self.image),
                'config_sha256':digest(self.config),'tests':tests,'metrics':{}}).encode()
        if name=='cleanup' and self.mode=='cleanup_failure': raise EvidenceError('model teardown failed')
        return b'{}'
    def run_model(self):
        atomic_json(self.station_path,self.station)
        return run(self.fixture_path,self.station_path,self.root,'uart_echo',self.static,
            self.report,self.leases,model=True,artifact_validator=self.artifacts,
            adapter_runner=self.adapter_runner)
    def test_model_success_never_claims_physical_qualification(self):
        result=self.run_model()
        self.assertEqual(result['status'],'pass')
        self.assertEqual(result['kind'],'board_fixture_model')
        self.assertIs(result['hardware_verified'],False)
        self.assertIs(result['eligible_physical_hil'],False)
        self.assertEqual(self.seen,list(self.station['commands']))
        self.assertFalse(any(self.leases.iterdir()))
    def test_injected_runner_is_blocked_in_physical_mode(self):
        atomic_json(self.station_path,self.station)
        result=run(self.fixture_path,self.station_path,self.root,'uart_echo',self.static,self.report,
            self.leases,execute_hardware=True,artifact_validator=self.artifacts,adapter_runner=self.adapter_runner)
        self.assertEqual(result['status'],'fail'); self.assertEqual(self.seen,[])
        self.assertIs(result['hardware_verified'],False)
    def test_wrong_observed_uid_blocks_flash(self):
        self.mode='wrong_uid'; result=self.run_model()
        self.assertEqual(result['status'],'fail'); self.assertEqual(self.seen,['identify','cleanup'])
    def test_readback_mismatch_blocks_reset(self):
        self.mode='bad_readback'; result=self.run_model()
        self.assertEqual(result['status'],'fail'); self.assertNotIn('reset',self.seen)
    def test_zero_tests_rejected(self):
        self.mode='zero_tests'; self.assertEqual(self.run_model()['status'],'fail')
    def test_skipped_hardware_test_rejected(self):
        self.mode='skipped_test'; self.assertEqual(self.run_model()['status'],'fail')
    def test_missing_physical_revision_blocks_all_commands(self):
        self.station['board']['revision']=None
        self.assertEqual(self.run_model()['status'],'fail'); self.assertEqual(self.seen,[])
    def test_missing_serial_blocks_all_commands(self):
        self.station['serial']['path']=''
        self.assertEqual(self.run_model()['status'],'fail'); self.assertEqual(self.seen,[])
    def test_wrong_board_profile_blocks_all_commands(self):
        self.station['board']['profile']='different-model-profile'
        self.assertEqual(self.run_model()['status'],'fail'); self.assertEqual(self.seen,[])
    def test_missing_reviewed_programmer_blocks_all_commands(self):
        self.station['probe']['tool']['path']=str(self.root/'missing-tool')
        self.assertEqual(self.run_model()['status'],'fail'); self.assertEqual(self.seen,[])
    def test_changed_adapter_is_rejected_before_commands(self):
        self.adapter.write_text('different code')
        self.assertEqual(self.run_model()['status'],'fail'); self.assertEqual(self.seen,[])
    def test_changed_binary_during_flash_is_rejected(self):
        self.mode='changed_image'; self.assertEqual(self.run_model()['status'],'fail')
        self.assertNotIn('reset',self.seen)
    def test_probe_alias_lease_blocks_new_logical_board(self):
        first={**self.station['board'],'id':'other-inventory-board'}
        with EquipmentLease(self.leases,first):
            self.assertEqual(self.run_model()['status'],'fail'); self.assertEqual(self.seen,[])
    def test_serial_alias_lease_blocks_different_probe(self):
        _,serial_key=validate_station(self.station,self.fixture,model=True)
        first={**self.station['board'],'id':'other-inventory-board','probe_serial':'other-model-probe'}
        with EquipmentLease(self.leases,first,serial_key):
            self.assertEqual(self.run_model()['status'],'fail'); self.assertEqual(self.seen,[])
    def test_failed_cleanup_quarantines_board_probe_and_serial(self):
        self.mode='cleanup_failure'; result=self.run_model()
        self.assertEqual(result['status'],'fail'); self.assertEqual(result['lease_status'],'quarantined')
        self.assertEqual(len(list(self.leases.glob('*/owner.json'))),3)
    def test_report_cannot_overwrite_binary(self):
        self.report=self.image; before=self.image.read_bytes()
        self.assertEqual(self.run_model()['status'],'fail'); self.assertEqual(self.image.read_bytes(),before)
    def test_reference_templates_do_not_have_fabricated_budgets(self):
        fixtures=[path for path in (ROOT/'scripts/hil/fixtures').glob('*.json')
                  if path.name not in ('station.template.json','fixture.schema.json')]
        self.assertEqual(len(fixtures),4)
        for path in fixtures:
            fixture=validate_fixture(json.loads(path.read_text()))
            self.assertIs(fixture['hardware_verified'],False)
            self.assertTrue(all(item['budget'] is None for item in fixture['pending_qualification']))
    def test_old_runner_is_dry_run_by_default(self):
        atomic_json(self.station_path,self.station)
        manifest={'schema_version':1,'kind':'physical_hil','lab_id':'physical-plan',
            'board':self.station['board'],'firmware':self.artifacts()['firmware'],
            'config_sha256':digest(self.config),'adapter_files':[file_identity(self.adapter)],
            'commands':self.station['commands'],'operation_timeout_s':2,'total_timeout_s':10,
            'required_tests':['uart_echo'],'budgets':{}}
        path=self.root/'physical-plan.json'; atomic_json(path,manifest)
        result=execute(path,self.report,self.leases)
        self.assertEqual(result['status'],'ready'); self.assertEqual(result['operations'],[])
        self.assertIs(result['hardware_verified'],False)
        self.assertFalse(self.leases.exists())
    def test_unknown_executable_blocks_legacy_preflight(self):
        manifest={'schema_version':1,'kind':'physical_hil','lab_id':'physical-plan',
            'board':self.station['board'],'firmware':self.artifacts()['firmware'],
            'config_sha256':digest(self.config),'adapter_files':[file_identity(self.adapter)],
            'commands':{name:['nexus-missing-review-tool'] for name in self.station['commands']},
            'operation_timeout_s':2,'total_timeout_s':10,'required_tests':['uart_echo'],'budgets':{}}
        path=self.root/'physical-plan.json'; atomic_json(path,manifest)
        result=execute(path,self.report,self.leases)
        self.assertEqual(result['status'],'fail'); self.assertEqual(result['operations'],[])
        self.assertIs(result['hardware_verified'],False)

class SerialEchoModels(unittest.TestCase):
    class Transport:
        def __init__(self,**kwargs): self.sent=bytearray()
        def __enter__(self): return self
        def __exit__(self,*args): return False
        def reset_input_buffer(self): pass
        def write(self,data): self.sent.extend(data); return len(data)
        def read(self,n): result=bytes(self.sent); self.sent.clear(); return result
    def test_echo_challenge_model_has_no_physical_claim(self):
        result=observe('model:serial',115200,0.1,model=True,serial_factory=self.Transport)
        self.assertEqual(result['status'],'pass'); self.assertIs(result['hardware_verified'],False)
        self.assertEqual(result['challenge_hex'],result['rx_hex'])
    def test_transport_injection_cannot_run_as_physical(self):
        result=observe('model:serial',115200,0.1,execute=True,serial_factory=self.Transport)
        self.assertEqual(result['status'],'fail'); self.assertIs(result['hardware_verified'],False)
    def test_model_without_injected_transport_cannot_open_real_serial(self):
        opened=[]
        def unexpected_transport(**kwargs):
            opened.append(kwargs)
            return self.Transport(**kwargs)
        with patch.dict(sys.modules,{'serial':types.SimpleNamespace(Serial=unexpected_transport)}):
            result=observe('model:serial',115200,0.1,model=True)
        self.assertEqual(opened,[])
        self.assertEqual(result['status'],'fail')
        self.assertIs(result['hardware_verified'],False)
    def test_default_uart_mode_does_not_open_a_port(self):
        result=observe('/not-connected/serial',115200,0.1)
        self.assertEqual(result['status'],'ready'); self.assertIs(result['hardware_verified'],False)

if __name__=='__main__': unittest.main()
