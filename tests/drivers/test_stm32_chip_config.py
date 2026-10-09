"""Compile/run the real chip validator against explicit effective-header cases."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
class ChipConfiguration(unittest.TestCase):
    def configuration(self,part='ZG',flash=1048576,physical_ram=196608,main_ram=131072):
        values={'NX_CONFIG_STM32_CHIP_NAME':'"STM32F407xx"',
                'NX_CONFIG_STM32_HAS_FPU':1,'NX_CONFIG_STM32_HAS_MPU':1,
                'NX_CONFIG_STM32_SYSCLK_FREQ':168000000,
                'NX_CONFIG_STM32_FLASH_SIZE':flash,'NX_CONFIG_STM32_SRAM_SIZE':physical_ram,
                'NX_CONFIG_LINKER_RAM_SIZE':main_ram}
        if part: values['NX_CONFIG_STM32F407'+part]=1
        return values
    def compile(self,values,*,valid):
        compiler=shutil.which(os.environ.get('CC','cc'))
        self.assertIsNotNone(compiler,'a real host C compiler is required; no skipped checks')
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            (root/'nexus_config.h').write_text('\n'.join(f'#define {name} {value}' for name,value in values.items())+'\n')
            executable=root/'chip-validator'
            command=[compiler,'-std=c11','-Wall','-Wextra','-Werror','-UNDEBUG',
                '-DSTM32F4','-DSTM32F407xx','-I'+str(root),'-I'+str(ROOT/'platforms/stm32/include'),
                str(ROOT/'tests/drivers/test_stm32_chip_config.c'),
                str(ROOT/'platforms/stm32/src/system/stm32_chip_validation.c'),'-o',str(executable)]
            result=subprocess.run(command,text=True,capture_output=True)
            if valid:
                self.assertEqual(result.returncode,0,result.stderr)
                executed=subprocess.run([str(executable)],text=True,capture_output=True)
                self.assertEqual(executed.returncode,0,executed.stderr)
            else:
                self.assertNotEqual(result.returncode,0,'invalid physical configuration was accepted')
    def test_ve512_and_physical192_main128(self): self.compile(self.configuration('VE',524288),valid=True)
    def test_vg1m_and_physical192_main128(self): self.compile(self.configuration('VG'),valid=True)
    def test_zg1m_and_smaller_main_allocation(self): self.compile(self.configuration('ZG',main_ram=65536),valid=True)
    def test_missing_flash_never_defaults(self):
        values=self.configuration(); del values['NX_CONFIG_STM32_FLASH_SIZE']; self.compile(values,valid=False)
    def test_missing_physical_ram_never_defaults(self):
        values=self.configuration(); del values['NX_CONFIG_STM32_SRAM_SIZE']; self.compile(values,valid=False)
    def test_main128_cannot_replace_physical192(self): self.compile(self.configuration(physical_ram=131072),valid=False)
    def test_physical192_cannot_be_linear_main_ram(self): self.compile(self.configuration(main_ram=196608),valid=False)
    def test_exact_part_is_required(self): self.compile(self.configuration(part=None),valid=False)
    def test_ve_cannot_take_xg_flash_density(self): self.compile(self.configuration('VE'),valid=False)
    def test_zg_cannot_take_xe_flash_density(self): self.compile(self.configuration('ZG',524288),valid=False)
if __name__=='__main__': unittest.main()
