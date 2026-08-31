#!/usr/bin/env python3
"""Embed SPIR-V binaries into a C++ header as byte arrays.

Usage: gen_spv_header.py <vertex.spv> <fragment.spv> <out_header.h>

Emits two `static const unsigned char[]` arrays (`gMCVertexSpv`, `gMCFragmentSpv`)
plus matching `_size` constants, ready to be passed to vkCreateShaderModule.
"""
import sys

def emit(var_name, spv_path):
    with open(spv_path, "rb") as f:
        data = f.read()
    lines = [f"static const unsigned char {var_name}[] = {{"]
    for i in range(0, len(data), 12):
        chunk = data[i:i + 12]
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")
    lines.append("};")
    lines.append(f"constexpr size_t {var_name}_size = {len(data)};")
    return "\n".join(lines) + "\n"

def main():
    vert_spv, frag_spv, out_header = sys.argv[1], sys.argv[2], sys.argv[3]
    header = (
        "// AUTO-GENERATED from SPIR-V by gen_spv_header.py. Do not edit.\n"
        "#pragma once\n#include <cstddef>\n\n"
        + emit("gMCVertexSpv", vert_spv)
        + "\n"
        + emit("gMCFragmentSpv", frag_spv)
    )
    with open(out_header, "w") as f:
        f.write(header)

if __name__ == "__main__":
    main()
