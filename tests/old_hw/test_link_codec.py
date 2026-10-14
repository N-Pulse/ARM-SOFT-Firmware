"""Decode the actual C++ writer with protobuf; assert additive host compatibility."""
from pathlib import Path
import ast,hashlib,sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'fw/comm-stack/PyUART'))
import protocol_messages_pb2 as pb
from google.protobuf import descriptor_pb2
wire=(ROOT/'build/old-hw-lot4-tests/link-wire.bin').read_bytes()
message=pb.ProsthesisMessage();message.ParseFromString(wire)
assert len(wire)==123 and message.WhichOneof('message')=='link'
for f in message.link.DESCRIPTOR.fields:
    assert getattr(message.link,f.name)==(2**64-1 if f.name.endswith('session') else 2**32-1),f.name
# Compare generated descriptors, allowing only the documented new outgoing message/oneof.
def read(path):
    for n in ast.walk(ast.parse(path.read_text())):
        if isinstance(n,ast.Call) and isinstance(n.func,ast.Attribute) and n.func.attr=='AddSerializedFile':
            return descriptor_pb2.FileDescriptorProto.FromString(ast.literal_eval(n.args[0]))
# Golden digest of the complete Lot 3 descriptor before this additive change.
# Keep the test reproducible without an ignored local baseline directory.
LOT3_DESCRIPTOR_SHA256='96ed93351a852e3ba8fed0674748d2dcf3e0f72fe03d2eda1d4a2aa52bb9b1a8'
new=read(ROOT/'fw/comm-stack/PyUART/protocol_messages_pb2.py')
for i in range(len(new.message_type)-1,-1,-1):
    if new.message_type[i].name=='LinkDiagnostic':del new.message_type[i]
parent=next(m for m in new.message_type if m.name=='ProsthesisMessage')
assert parent.field[-1].name=='link' and parent.field[-1].number==5
del parent.field[-1]
assert hashlib.sha256(new.SerializeToString()).hexdigest()==LOT3_DESCRIPTOR_SHA256
print('PASS additive host descriptor, actual C++ -> Python link status, 123-byte worst payload; no serial device')
