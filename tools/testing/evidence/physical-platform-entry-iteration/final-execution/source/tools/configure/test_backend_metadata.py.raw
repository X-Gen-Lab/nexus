"""Compatibility metadata advertises names without broadening CPU admission."""
import unittest
try:
    from . import metadata, threadx
except ImportError:
    import metadata
    import threadx


class BackendMetadataTest(unittest.TestCase):
    def policy(self):
        policy = metadata.read_json(metadata.ROOT / metadata.POLICY)
        policy['configured_backends'] = ['baremetal', 'native', 'freertos', 'threadx']
        return policy

    def test_known_backend_names_are_accepted(self):
        policy = self.policy()
        self.assertEqual(metadata.policy_contract(policy), policy)

    def test_unknown_duplicate_and_non_names_are_rejected(self):
        for names in [['threadx', 'other'], ['threadx', 'threadx'], 'threadx',
                      [], [True], ['rtx']]:
            with self.subTest(names=names):
                policy = self.policy()
                policy['configured_backends'] = names
                with self.assertRaises(ValueError):
                    metadata.policy_contract(policy)

    def test_names_do_not_authorize_an_unreviewed_board(self):
        metadata.policy_contract(self.policy())
        with self.assertRaises(ValueError):
            threadx.validate_platform('unknown-pcb', 'stm32f407')

    def test_original_policy_shape_remains_accepted(self):
        policy = self.policy()
        del policy['configured_backends']
        self.assertEqual(metadata.policy_contract(policy), policy)
