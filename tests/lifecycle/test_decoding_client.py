"""Offline tests for the real current host link; no pyserial/network needed."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
import decoding_link as d
from intent_actions import ACTION_NAMES
import ast
import re

class CurrentPeer(unittest.TestCase):
    def test_known_crc_and_handshake(self):
        self.assertEqual(d.crc16(bytes([0,0,1])),0xdcbd)
        peer=d.Peer(action=17)
        self.assertIsNone(peer.service(0))
        peer.receive(d.packet(2,0,d.Parameters().wire()),0)
        self.assertFalse(peer.active)
        hello=peer.receive(d.SYNC,0)
        self.assertEqual(hello,d.hello_frame(1,d.Parameters()))
        peer.receive(d.packet(2,1,d.Parameters().wire()),.001)
        self.assertTrue(peer.active)
        f=peer.service(.001)
        self.assertEqual(f[0],0x10)
        self.assertEqual(f[5:-2],bytes([0x12,4,0x0a,2,8,17]))
        self.assertIsNone(peer.service(.002))
        self.assertIsNotNone(peer.service(.032))
    def test_nack_reconnect_and_bounds(self):
        peer=d.Peer(action=18)
        peer.receive(d.SYNC,0)
        p=d.Parameters(1,1,6,20)
        peer.receive(d.packet(3,1,p.wire()),.001)
        self.assertEqual(peer.parameters,p)
        peer.service(.001)
        self.assertIsNotNone(peer.service(.011))
        peer.receive(d.SYNC,.02); self.assertFalse(peer.active)
        self.assertIsNone(peer.service(.03))
        peer.receive(d.packet(2,1,p.wire()),.031); self.assertFalse(peer.active)
        peer.receive(d.packet(2,2,p.wire()),.032); self.assertTrue(peer.active)
        with self.assertRaises(ValueError): d.data_frame(19,0,p)
        with self.assertRaises(ValueError): d.data_frame(0,0,d.Parameters(data_len=0))
        self.assertEqual(len(d.data_frame(0,0,p)),12)
    def test_parser_noise_fragmentation_and_crc(self):
        parser=d.Parser(); frame=d.packet(2,1,d.Parameters().wire())
        self.assertEqual(parser.feed(b'xyz'+d.SYNC[:2],0),[])
        self.assertEqual(parser.feed(d.SYNC[2:]+frame,.001),[d.SYNC,frame])
        bad=frame[:-1]+bytes([frame[-1]^1])
        self.assertEqual(parser.feed(bad,.002),[])
        self.assertEqual(parser.feed(d.SYNC,.2),[d.SYNC])
    def test_schema_and_python_descriptors(self):
        root=Path(__file__).resolve().parents[2]
        schema=(root/'fw/comm-stack/protocol_messages.proto').read_text().split('enum Action {',1)[1].split('}',1)[0]
        values=[(name,int(number)) for name,number in re.findall(r'(\w+)\s*=\s*(\d+)\s*;',schema)]
        self.assertEqual(values,[(name,i) for i,name in enumerate(ACTION_NAMES)])
        descriptors=[]
        for path in ('fw/comm-stack/PyUART/protocol_messages_pb2.py','fw/comm-stack/comm_ws/src/bmi_gateway/bmi_gateway/protocol_messages_pb2.py'):
            tree=ast.parse((root/path).read_text())
            node=next(n for n in ast.walk(tree) if isinstance(n,ast.Call) and isinstance(n.func,ast.Attribute) and n.func.attr=='AddSerializedFile')
            wire=ast.literal_eval(node.args[0]); descriptors.append(wire)
            for name,number in values:
                body=bytes([10,len(name)])+name.encode()+bytes([16,number])
                self.assertIn(bytes([18,len(body)])+body,wire)
        self.assertEqual(descriptors[0],descriptors[1])

    def test_explicit_action_vectors(self):
        p=d.Parameters()
        for action in range(19):
            f=d.data_frame(action,255,p)
            self.assertEqual(f[1],255)
            self.assertEqual(f[-3],action)
            self.assertEqual(int.from_bytes(f[-2:],'little'),d.crc16(f[:-2]))
if __name__=='__main__': unittest.main()
