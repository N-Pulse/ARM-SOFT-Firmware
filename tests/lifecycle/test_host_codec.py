"""Wire migration checks without protobuf, pyserial, or physical ports."""
from pathlib import Path
import importlib.util
import unittest

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('lifecycle_client',ROOT/'tools/lifecycle_client.py')
client=importlib.util.module_from_spec(spec)
spec.loader.exec_module(client)

class ProtocolMigration(unittest.TestCase):
    def test_control_uses_field_six(self):
        self.assertEqual(client.control_frame(3,7,1),bytes.fromhex('aa083206080310071801'))
        self.assertEqual(client.control_frame(6,1,1)[2],0x32)

    def test_hello_and_actions_unchanged(self):
        self.assertEqual(client.hello_frame(),bytes.fromhex('aa080a06080110011801'))
        self.assertEqual(client.action_frame(0),bytes.fromhex('aa0412020a00'))
        for action in range(1,17):
            self.assertEqual(client.action_frame(action),bytes([0xaa,6,0x12,4,0x0a,2,8,action]))

    def test_lifecycle_responses_are_unambiguous(self):
        for tag in (5,6,7,8):
            payload=client.message(tag,{1:5,2:123,3:1})[2:]
            self.assertEqual(client.decode(payload),(tag,{1:5,2:123,3:1}))
        self.assertIn('state=READY',client.describe(5,{1:3}))
        self.assertIn('SAFETY',client.describe(6,{1:1,2:2,3:2,5:5}))

    def test_legacy_float_diagnostic_is_ignored(self):
        # Legacy field 3 contains a float; it must never be parsed as status.
        self.assertEqual(client.decode(bytes.fromhex('1a05350000803f')),(3,{}))
        self.assertEqual(client.decode(bytes.fromhex('220408011002')),(4,{}))

    def test_schema_allocations_match_client(self):
        legacy=(ROOT/'fw/comm-stack/protocol_messages.proto').read_text()
        extension=(ROOT/'fw/comm-stack/lifecycle_messages.proto').read_text()
        for name,tag in [('commission',3),('stop',4),('diagnostic',5)]:
            device=legacy.split('message DeviceMessage {',1)[1].split('\n}',1)[0]
            self.assertRegex(device,rf'\b{name}\s*=\s*{tag}\s*;')
        for typename,fields in [
            ('LifecycleDeviceMessage',{'commission':3,'stop':4,'diagnostic':5,'control':6}),
            ('LifecycleProsthesisMessage',{'diagnostic':3,'reply':4,'status':5,'safety':6,'control':7,'hello':8})]:
            block=extension.split('message '+typename+' {',1)[1].split('\n}',1)[0]
            for name,tag in fields.items(): self.assertRegex(block,rf'\b{name}\s*=\s*{tag}\s*;')

if __name__=='__main__': unittest.main()
