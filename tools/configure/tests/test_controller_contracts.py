"""One immutable controller vocabulary drives authoring and C interface views."""
from dataclasses import FrozenInstanceError
import unittest

from tools.configure import authored, configure, dependencies, factory, selection
from tools.configure.contracts import CONTROLLER_CONTRACTS, CONTROLLER_KINDS


class ControllerVocabularyTests(unittest.TestCase):
    def test_consumers_share_exact_contract_objects(self):
        self.assertEqual(tuple(CONTROLLER_CONTRACTS), CONTROLLER_KINDS)
        self.assertIs(authored.INSTANCE_FIELDS['uart'],
                      CONTROLLER_CONTRACTS['uart'].authored_fields)
        self.assertEqual(authored.KINDS, frozenset(CONTROLLER_KINDS))
        self.assertEqual(configure.KINDS, frozenset(CONTROLLER_KINDS))
        self.assertIs(factory.KINDS, CONTROLLER_KINDS)
        for kind, contract in CONTROLLER_CONTRACTS.items():
            with self.subTest(kind=kind):
                self.assertEqual(factory.public_header(kind), contract.header)
                self.assertIsInstance(contract.authored_fields, frozenset)
                self.assertIsInstance(contract.selected_fields, frozenset)

    def test_contract_is_declared_for_runtime_sdk_and_freshness(self):
        for sdk in (False, True):
            self.assertIn("tools/configure/contracts.py",
                          dependencies.source_names("runtime", "native", sdk=sdk))

    def test_contracts_cannot_be_mutated_by_a_consumer(self):
        with self.assertRaises(TypeError):
            CONTROLLER_CONTRACTS['foreign'] = CONTROLLER_CONTRACTS['uart']
        with self.assertRaises(FrozenInstanceError):
            CONTROLLER_CONTRACTS['uart'].header = 'foreign'
        with self.assertRaises(TypeError):
            authored.INSTANCE_FIELDS["foreign"] = frozenset()
        with self.assertRaises(AttributeError):
            authored.INSTANCE_FIELDS['uart'].add('foreign')

    def test_lptim_mode_specific_irq_does_not_mutate_shared_contract(self):
        before = CONTROLLER_CONTRACTS['lptim'].selected_fields
        route = {'kind': 'lptim'}
        provider = object()
        with self.assertRaises(ValueError):
            selection.validate_selection({'mode': 'irq-counter16-oneshot',
                'tick_hz': 1, 'irq_priority': 5, 'calls_os': False,
                'foreign': True}, route, provider, 4096)
        self.assertEqual(CONTROLLER_CONTRACTS['lptim'].selected_fields, before)
        self.assertNotIn('irq_priority', before)


if __name__ == '__main__':
    unittest.main()
