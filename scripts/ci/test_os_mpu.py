"""Negative checks for real MPU ELF region and syscall evidence."""
import struct
import unittest
import os_mpu


class MpuEvidence(unittest.TestCase):
    def test_exact_two_actual_impl_addresses_and_no_other_slots(self):
        values = [0] * 70
        values[6], values[13] = 0x08000011, 0x08000021
        data = struct.pack('<70I', *values)
        os_mpu.check_syscall_table(data, 0x08000010, 0x08000020)
        values[14] = 0x08000031
        with self.assertRaises(ValueError):
            os_mpu.check_syscall_table(struct.pack('<70I', *values), 0x08000010, 0x08000020)
        with self.assertRaises(ValueError):
            os_mpu.check_syscall_table(data[:-4], 0x08000010, 0x08000020)

    def test_privileged_and_user_code_regions_are_not_interchangeable(self):
        symbols = {
            '__nexus_privileged_flash_start__': (0x08000000, 0),
            '__nexus_privileged_flash_end__': (0x08080000, 0),
            '__nexus_user_flash_start__': (0x08080000, 0),
            '__nexus_user_flash_end__': (0x08090000, 0),
            '__nexus_syscall_flash_start__': (0x08090000, 0),
            '__nexus_syscall_flash_end__': (0x080a0000, 0),
            '__nexus_privileged_ram_start__': (0x20000000, 0),
            '__nexus_privileged_ram_end__': (0x20020000, 0),
            'nx_freertos_mpu_task_start': (0x08000001, 32),
            'nx_freertos_mpu_task_delete': (0x08000021, 32),
            'nx_freertos_mpu_layout': (0x08000041, 32),
            'nx_freertos_permanent_task_start': (0x08000061, 32),
            'MPU_vTaskDelayImpl': (0x08000081, 32),
            'MPU_xTaskGetTickCountImpl': (0x080000a1, 32),
            'nx_freertos_user_delay': (0x08080001, 32),
            'nx_freertos_user_ticks': (0x08080021, 32),
            'user_entry': (0x08080041, 32),
            'MPU_vTaskDelay': (0x08090001, 32),
            'MPU_xTaskGetTickCount': (0x08090021, 32),
            'uxSystemCallImplementations': (0x20000000, 280),
            'task': (0x20000120, 1024),
            'idle_task': (0x20000800, 1024),
        }
        os_mpu.check_domains(symbols)
        symbols['nx_freertos_mpu_task_start'] = (0x08080080, 32)
        with self.assertRaises(ValueError):
            os_mpu.check_domains(symbols)

    def test_real_symbol_sizes_are_required(self):
        parsed = os_mpu.symbols('  12: 20000000   280 OBJECT GLOBAL DEFAULT    4 uxSystemCallImplementations\n')
        self.assertEqual(parsed, {'uxSystemCallImplementations': (0x20000000, 280)})
        with self.assertRaises(ValueError):
            os_mpu.check_domains(parsed)


if __name__ == '__main__':
    unittest.main()
