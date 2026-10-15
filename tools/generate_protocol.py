"""Regenerate both Python bindings and EmbeddedProto from the same schema.
Requires grpcio-tools and the bundled EmbeddedProto generator's dependencies.
No protoc executable or modifications to third-party sources required.
"""
from pathlib import Path
import shutil
import sys
from grpc_tools import protoc
import grpc_tools
from google.protobuf import descriptor_pb2
from google.protobuf.compiler import plugin_pb2

ROOT = Path(__file__).resolve().parents[1]

def main():
    out = ROOT / 'build/protocol-generator'
    generator = ROOT / 'Middlewares/Third_Party/EmbeddedProto/generator'
    out.mkdir(parents=True, exist_ok=True)
    shutil.copytree(generator/'EmbeddedProto', out/'EmbeddedProto', dirs_exist_ok=True)
    include = Path(grpc_tools.__file__).parent/'_proto'
    assert protoc.main(['protoc', f'-I{generator}', f'-I{include}',
        f'--python_out={out / "EmbeddedProto"}', 'embedded_proto_options.proto']) == 0
    schema = ROOT/'fw/comm-stack'
    assert protoc.main(['protoc', f'-I{schema}', f'--descriptor_set_out={out / "schema.pb"}',
        '--include_imports', f'--python_out={schema / "PyUART"}', 'protocol_messages.proto']) == 0
    sys.path.insert(0, str(out))
    from EmbeddedProto.main import generate_code
    descriptor = descriptor_pb2.FileDescriptorSet.FromString((out/'schema.pb').read_bytes())
    request = plugin_pb2.CodeGeneratorRequest()
    request.file_to_generate.append('protocol_messages.proto')
    request.proto_file.extend(descriptor.file)
    response = plugin_pb2.CodeGeneratorResponse()
    generate_code(request, response)
    for file in response.file:
        (schema/file.name).write_text('\n'.join(line.rstrip() for line in file.content.splitlines())+'\n',encoding='utf-8')
    shutil.copy2(schema/'PyUART/protocol_messages_pb2.py',
        schema/'comm_ws/src/bmi_gateway/bmi_gateway/protocol_messages_pb2.py')
    print('Regenerated EmbeddedProto and both Python bindings')

if __name__ == '__main__': main()
