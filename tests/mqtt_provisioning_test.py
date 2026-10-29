"""Pure scope validation; not broker authentication or physical-board proof."""
import importlib.util
from pathlib import Path
import sys
import unittest

sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('provision_mqtt_device', Path(__file__).resolve().parents[1] / 'tools/provision-mqtt-device.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ProvisioningScopeTest(unittest.TestCase):
    def test_existing_default_preserved(self):
        self.assertEqual(module.provisioning_scope('ledboard'), ('ledboard-mqtt.json',
            {'device': 'pakar-ledboard', 'control': 'pakar-ledboard-control'}, 'pakarcontroller/ledboard'))

    def test_boards_have_independent_principals_and_topics(self):
        scopes = [module.provisioning_scope(name) for name in ['ledcontroller', 'keycontroller']]
        self.assertNotEqual(scopes[0][0], scopes[1][0])
        self.assertNotEqual(scopes[0][1], scopes[1][1])
        self.assertNotEqual(scopes[0][2], scopes[1][2])
        for _, accounts, _ in scopes:
            self.assertTrue(all(len(name) <= 32 for name in accounts.values()))

    def test_rejects_wildcards_paths_and_ambiguous_names(self):
        for name in ['', '../keycontroller', 'key/controller', 'key#', '+', 'KEY', 'key\n', 'a' * 21, None]:
            with self.subTest(name=name), self.assertRaises(ValueError):
                module.provisioning_scope(name)


if __name__ == '__main__':
    unittest.main()
