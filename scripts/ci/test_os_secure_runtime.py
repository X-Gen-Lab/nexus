"""Reject incomplete or forged split-world software qualification evidence."""
import unittest
import os_secure_runtime as secure


class SecureEvidence(unittest.TestCase):
    def table(self):
        value = {name: (0x10010000 + 8 * index, 8)
                 for index, name in enumerate(secure.GATEWAYS)}
        value.update({'SecureContext_LoadContextAsm': (0x10000001, 8),
                      'SecureContext_SaveContextAsm': (0x10000011, 8)})
        return value

    def test_actual_veneers_and_both_real_asm_symbols_are_required(self):
        table = self.table()
        secure.check_secure(table, 0x10010000, 64)
        del table['SecureContext_SaveContextAsm']
        with self.assertRaises(ValueError):
            secure.check_secure(table, 0x10010000, 64)

    def test_gateway_address_must_be_in_real_nsc_output(self):
        table = self.table()
        table['SecureContext_Init'] = (0x10000000, 8)
        with self.assertRaises(ValueError):
            secure.check_secure(table, 0x10010000, 64)

    def test_each_gateway_has_its_own_exported_veneer(self):
        table = self.table()
        table['SecureContext_Init'] = table['SecureContext_FreeContext']
        with self.assertRaises(ValueError):
            secure.check_secure(table, 0x10010000, 64)

    def test_context_heap_and_default_maximum_pool_are_rejected(self):
        for name in ('pvPortMalloc', 'malloc', 'xSecureContexts'):
            table = self.table()
            table[name] = (0x30000000, 1024)
            with self.assertRaises(ValueError):
                secure.check_secure(table, 0x10010000, 64)

    def test_fixed_seal_copy_has_no_external_memory_helper(self):
        secure.check_context_helpers(' U nx_arch_dsb\n')
        for name in ('memcpy', '__aeabi_memcpy8', 'memset'):
            with self.assertRaises(ValueError):
                secure.check_context_helpers(' U ' + name + '\n')

    def test_nonsecure_consumes_exact_imported_gateway_addresses(self):
        secured = self.table()
        imported = {name: secured[name] for name in secure.GATEWAYS}
        imported.update({name: (0x20000 + index * 16, 16)
                         for index, name in enumerate(secure.NS_REQUIRED)})
        secure.check_nonsecure(imported, secured)
        imported['SecureContext_AllocateContext'] = (0x100100f0, 0)
        with self.assertRaises(ValueError):
            secure.check_nonsecure(imported, secured)

    def test_ns_actual_kernel_and_caller_owned_idle_are_required(self):
        secured = self.table()
        imported = {name: secured[name] for name in secure.GATEWAYS}
        with self.assertRaises(ValueError):
            secure.check_nonsecure(imported, secured)

    def test_matrix_uses_every_reviewed_v8_optional_abi_once(self):
        pairs = secure.matrix()
        self.assertEqual(len(pairs), 31)
        names = {item['name'] for item in pairs}
        self.assertEqual(len(names), len(pairs))
        for item in pairs:
            secured, nonsecure = item['secure'], item['nonsecure']
            a = secured['profile']['compile_options']
            b = nonsecure['profile']['compile_options']
            self.assertIn('-mcmse', a)
            self.assertNotIn('-mcmse', b)
            self.assertEqual([flag for flag in a if flag != '-mcmse'], b)
            self.assertEqual(nonsecure['kernel']['security_model'], 'split')
            self.assertFalse(nonsecure['kernel']['memory_protection'])


if __name__ == '__main__':
    unittest.main()
