"""
Extract vertex positions and triangle indices from GLB files.
Outputs a C++ header with static const arrays for each mesh.

Usage: python extract_meshes.py
"""

import struct
import json
import os
import sys
from pathlib import Path

MESH_DIR = Path(r"C:\Users\kyle\Developer\PuckAssets\mesh")
OUTPUT_DIR = Path(r"C:\Users\kyle\Developer\PuckSim\Source")

# Meshes to extract: (glb_filename, c_identifier, shape_type)
MESHES = [
    ("Puck Collider (Stick).glb",          "PUCK_STICK_COLLIDER",       "convex"),
    ("Puck Collider (Level).glb",          "PUCK_LEVEL_COLLIDER",       "convex"),
    ("Stick Shaft Collider (Attacker).glb", "STICK_SHAFT_COLLIDER",     "convex"),
    ("Stick Blade Collider (Attacker).glb", "STICK_BLADE_COLLIDER",     "convex"),
    ("Barrier Collider.glb",               "BARRIER_COLLIDER",          "triangle"),
    ("Frame.glb",                          "GOAL_FRAME",                "triangle"),
    ("Net Collider.glb",                   "GOAL_NET_COLLIDER",         "triangle"),
    ("Goal Trigger.glb",                   "GOAL_TRIGGER",              "convex"),
    ("Goal Player Collider.glb",           "GOAL_PLAYER_COLLIDER",      "convex"),
    ("Groin Collider (Attacker).glb",      "GROIN_COLLIDER",            "convex"),
    ("Torso Collider (Attacker).glb",      "TORSO_COLLIDER",            "convex"),
]

# GLB constants
GLB_MAGIC = 0x46546C67  # 'glTF'
CHUNK_JSON = 0x4E4F534A
CHUNK_BIN = 0x004E4942

# glTF component types
COMPONENT_TYPES = {
    5120: ('b', 1),  # BYTE
    5121: ('B', 1),  # UNSIGNED_BYTE
    5122: ('h', 2),  # SHORT
    5123: ('H', 2),  # UNSIGNED_SHORT
    5125: ('I', 4),  # UNSIGNED_INT
    5126: ('f', 4),  # FLOAT
}

# glTF accessor types -> component count
TYPE_COUNTS = {
    "SCALAR": 1,
    "VEC2": 2,
    "VEC3": 3,
    "VEC4": 4,
    "MAT2": 4,
    "MAT3": 9,
    "MAT4": 16,
}


def parse_glb(filepath):
    """Parse a GLB file and return (json_data, binary_data)."""
    with open(filepath, 'rb') as f:
        # Header: magic(4) + version(4) + length(4)
        magic, version, length = struct.unpack('<III', f.read(12))
        if magic != GLB_MAGIC:
            raise ValueError(f"Not a valid GLB file: {filepath}")

        json_data = None
        bin_data = None

        while f.tell() < length:
            chunk_length, chunk_type = struct.unpack('<II', f.read(8))
            chunk_data = f.read(chunk_length)

            if chunk_type == CHUNK_JSON:
                json_data = json.loads(chunk_data)
            elif chunk_type == CHUNK_BIN:
                bin_data = chunk_data

    return json_data, bin_data


def read_accessor(gltf, bin_data, accessor_index):
    """Read data from a glTF accessor."""
    accessor = gltf['accessors'][accessor_index]
    buffer_view = gltf['bufferViews'][accessor['bufferView']]

    component_type = accessor['componentType']
    accessor_type = accessor['type']
    count = accessor['count']

    fmt_char, byte_size = COMPONENT_TYPES[component_type]
    num_components = TYPE_COUNTS[accessor_type]

    # Calculate offsets
    byte_offset = buffer_view.get('byteOffset', 0) + accessor.get('byteOffset', 0)
    byte_stride = buffer_view.get('byteStride', byte_size * num_components)

    values = []
    for i in range(count):
        offset = byte_offset + i * byte_stride
        components = []
        for j in range(num_components):
            val = struct.unpack_from(f'<{fmt_char}', bin_data, offset + j * byte_size)[0]
            components.append(val)
        if num_components == 1:
            values.append(components[0])
        else:
            values.append(tuple(components))

    return values


def extract_mesh(filepath):
    """Extract vertices and indices from a GLB file."""
    gltf, bin_data = parse_glb(filepath)

    if not gltf.get('meshes'):
        raise ValueError(f"No meshes found in {filepath}")

    all_vertices = []
    all_indices = []
    vertex_offset = 0

    for mesh in gltf['meshes']:
        for primitive in mesh['primitives']:
            # Get position accessor
            pos_accessor_idx = primitive['attributes']['POSITION']
            positions = read_accessor(gltf, bin_data, pos_accessor_idx)
            all_vertices.extend(positions)

            # Get index accessor (if present)
            if 'indices' in primitive:
                indices = read_accessor(gltf, bin_data, primitive['indices'])
                all_indices.extend(idx + vertex_offset for idx in indices)
            else:
                # No index buffer — sequential triangles
                all_indices.extend(range(vertex_offset, vertex_offset + len(positions)))

            vertex_offset += len(positions)

    return all_vertices, all_indices


def format_vertex(v):
    """Format a vertex tuple as C++ initializer."""
    return f"{{{v[0]:.8f}f, {v[1]:.8f}f, {v[2]:.8f}f}}"


def generate_header(mesh_data):
    """Generate C++ header content from extracted mesh data."""
    lines = []
    lines.append("#pragma once")
    lines.append("// Auto-generated by tools/extract_meshes.py")
    lines.append("// Source: https://github.com/ckhawks/PuckAssets/tree/main/mesh")
    lines.append("//")
    lines.append("// Do not edit manually. Re-run the extraction script to regenerate.")
    lines.append("")
    lines.append("#include <Jolt/Jolt.h>")
    lines.append("JPH_SUPPRESS_WARNINGS")
    lines.append("")
    lines.append("using namespace JPH;")
    lines.append("")

    for name, vertices, indices, shape_type in mesh_data:
        num_verts = len(vertices)
        num_indices = len(indices)
        num_tris = num_indices // 3

        lines.append(f"// {name}: {num_verts} vertices, {num_tris} triangles ({shape_type})")

        # Vertices
        lines.append(f"static constexpr int {name}_NUM_VERTICES = {num_verts};")
        lines.append(f"static constexpr Float3 {name}_VERTICES[{num_verts}] = {{")
        for i, v in enumerate(vertices):
            comma = "," if i < num_verts - 1 else ""
            lines.append(f"\t{format_vertex(v)}{comma}")
        lines.append("};")
        lines.append("")

        # Indices
        lines.append(f"static constexpr int {name}_NUM_INDICES = {num_indices};")
        lines.append(f"static constexpr uint32_t {name}_INDICES[{num_indices}] = {{")

        # Print indices in groups of 3 (one triangle per line)
        for i in range(0, num_indices, 3):
            tri = indices[i:i+3]
            comma = "," if i + 3 < num_indices else ""
            if len(tri) == 3:
                lines.append(f"\t{tri[0]}, {tri[1]}, {tri[2]}{comma}")
            else:
                lines.append(f"\t{', '.join(str(x) for x in tri)}{comma}")
        lines.append("};")
        lines.append("")

    return "\n".join(lines)


def main():
    mesh_data = []
    errors = []

    print(f"Extracting meshes from: {MESH_DIR}")
    print(f"Output to: {OUTPUT_DIR / 'MeshData.h'}")
    print()

    for filename, c_name, shape_type in MESHES:
        filepath = MESH_DIR / filename
        if not filepath.exists():
            errors.append(f"  MISSING: {filename}")
            continue

        try:
            vertices, indices = extract_mesh(filepath)
            num_tris = len(indices) // 3
            mesh_data.append((c_name, vertices, indices, shape_type))
            print(f"  OK: {filename}")
            print(f"      {len(vertices)} vertices, {num_tris} triangles ({shape_type})")

            # Print AABB for verification
            xs = [v[0] for v in vertices]
            ys = [v[1] for v in vertices]
            zs = [v[2] for v in vertices]
            print(f"      AABB: X[{min(xs):.4f}, {max(xs):.4f}] "
                  f"Y[{min(ys):.4f}, {max(ys):.4f}] "
                  f"Z[{min(zs):.4f}, {max(zs):.4f}]")
        except Exception as e:
            errors.append(f"  ERROR: {filename}: {e}")

    if errors:
        print()
        print("Errors:")
        for e in errors:
            print(e)

    if mesh_data:
        header = generate_header(mesh_data)
        output_path = OUTPUT_DIR / "MeshData.h"
        with open(output_path, 'w') as f:
            f.write(header)
        print()
        print(f"Generated: {output_path}")
        print(f"  {len(mesh_data)} meshes, "
              f"{sum(len(v) for _, v, _, _ in mesh_data)} total vertices, "
              f"{sum(len(i) for _, _, i, _ in mesh_data) // 3} total triangles")
    else:
        print("No meshes extracted!")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
