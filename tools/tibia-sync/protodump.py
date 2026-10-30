"""Find serialized FileDescriptorProtos in a binary and render them as .proto text."""

from __future__ import annotations

from google.protobuf import descriptor_pb2

# FileDescriptorProto field number -> wire type.
FILE_FIELDS = {1: 2, 2: 2, 3: 2, 4: 2, 5: 2, 6: 2, 7: 2, 8: 2, 9: 2, 10: 0, 11: 0, 12: 2, 14: 0}

SCALARS = {
    descriptor_pb2.FieldDescriptorProto.TYPE_DOUBLE: "double",
    descriptor_pb2.FieldDescriptorProto.TYPE_FLOAT: "float",
    descriptor_pb2.FieldDescriptorProto.TYPE_INT64: "int64",
    descriptor_pb2.FieldDescriptorProto.TYPE_UINT64: "uint64",
    descriptor_pb2.FieldDescriptorProto.TYPE_INT32: "int32",
    descriptor_pb2.FieldDescriptorProto.TYPE_FIXED64: "fixed64",
    descriptor_pb2.FieldDescriptorProto.TYPE_FIXED32: "fixed32",
    descriptor_pb2.FieldDescriptorProto.TYPE_BOOL: "bool",
    descriptor_pb2.FieldDescriptorProto.TYPE_STRING: "string",
    descriptor_pb2.FieldDescriptorProto.TYPE_BYTES: "bytes",
    descriptor_pb2.FieldDescriptorProto.TYPE_UINT32: "uint32",
    descriptor_pb2.FieldDescriptorProto.TYPE_SFIXED32: "sfixed32",
    descriptor_pb2.FieldDescriptorProto.TYPE_SFIXED64: "sfixed64",
    descriptor_pb2.FieldDescriptorProto.TYPE_SINT32: "sint32",
    descriptor_pb2.FieldDescriptorProto.TYPE_SINT64: "sint64",
}
LABELS = {1: "optional", 2: "required", 3: "repeated"}


def _varint(buf, pos):
    value = shift = 0
    while pos < len(buf):
        b = buf[pos]
        pos += 1
        value |= (b & 0x7F) << shift
        if not b & 0x80:
            return value, pos
        shift += 7
        if shift > 63:
            break
    raise ValueError("bad varint")


def _extent(buf, start, limit=1 << 20):
    """End of the longest run of valid FileDescriptorProto fields from start."""
    pos, end = start, start
    stop = min(len(buf), start + limit)
    while pos < stop:
        try:
            tag, p = _varint(buf, pos)
            field, wire = tag >> 3, tag & 7
            if FILE_FIELDS.get(field) != wire:
                break
            if wire == 0:
                _, p = _varint(buf, p)
            else:
                n, p = _varint(buf, p)
                p += n
            if p > stop:
                break
        except ValueError:
            break
        pos = end = p
    return end


def find_descriptors(buf, package_prefix="tibia."):
    found = {}
    at = buf.find(b".proto")
    while at != -1:
        # Field 1 (name): 0x0a, length, name; the name ends at ".proto".
        for back in range(1, 128):
            s = at + len(".proto") - back - 2
            if s < 0 or buf[s] != 0x0A or buf[s + 1] != back:
                continue
            end = _extent(buf, s)
            try:
                fd = descriptor_pb2.FileDescriptorProto.FromString(buf[s:end])
            except Exception:
                continue
            if fd.name.endswith(".proto") and fd.package.startswith(package_prefix) and (fd.message_type or fd.enum_type):
                found.setdefault(fd.name, fd)
            break
        at = buf.find(b".proto", at + 1)
    return found


def _type(field):
    if field.type_name:
        return field.type_name.lstrip(".")
    return SCALARS[field.type]


def _field(field, indent):
    opts = []
    if field.HasField("default_value"):
        quoted = field.type in (descriptor_pb2.FieldDescriptorProto.TYPE_STRING, descriptor_pb2.FieldDescriptorProto.TYPE_BYTES)
        opts.append(f'default = "{field.default_value}"' if quoted else f"default = {field.default_value}")
    if field.options.HasField("packed"):
        opts.append(f"packed = {'true' if field.options.packed else 'false'}")
    suffix = f" [{', '.join(opts)}]" if opts else ""
    return f"{indent}{LABELS[field.label]} {_type(field)} {field.name} = {field.number}{suffix};"


def _enum(enum, indent):
    lines = [f"{indent}enum {enum.name} {{"]
    lines += [f"{indent}  {v.name} = {v.number};" for v in enum.value]
    lines.append(f"{indent}}}")
    return lines


def _message(msg, indent):
    lines = [f"{indent}message {msg.name} {{"]
    inner = indent + "  "
    for e in msg.enum_type:
        lines += _enum(e, inner)
    for m in msg.nested_type:
        lines += _message(m, inner)
    oneofs = {}
    for f in msg.field:
        if f.HasField("oneof_index"):
            oneofs.setdefault(f.oneof_index, []).append(f)
        else:
            lines.append(_field(f, inner))
    for index, fields in oneofs.items():
        lines.append(f"{inner}oneof {msg.oneof_decl[index].name} {{")
        lines += [_field(f, inner + "  ").replace("optional ", "", 1) for f in fields]
        lines.append(f"{inner}}}")
    lines.append(f"{indent}}}")
    return lines


def render(fd):
    lines = ["// extracted from the Tibia client", f'syntax = "{fd.syntax or "proto2"}";', f"package {fd.package};"]
    lines += [f'import "{d}";' for d in fd.dependency]
    for e in fd.enum_type:
        lines += _enum(e, "")
    for m in fd.message_type:
        lines += _message(m, "")
    return "\n".join(lines) + "\n"
