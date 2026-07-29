#!/usr/bin/env python3
"""Convert the banner glTF and apply its CGFX-only logo billboard flag.

The proven SM64 banner stores its screen-facing ``name`` bone as a Y-axis
billboard (mode 5).  glTF cannot represent that CGFX flag, so it is applied
after conversion while the logo's complete transform remains baked into its
vertices.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


RIGID_ANIMATED_MESHES = {
    "Cloud_Textured",
    "Front_Decorations",
    "Headphones",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("input_gltf", type=Path)
    parser.add_argument("output_cgfx", type=Path)
    parser.add_argument(
        "--pycgfx-root",
        type=Path,
        default=Path(".tools/pycgfx"),
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    pycgfx_root = args.pycgfx_root.resolve()
    if not (pycgfx_root / "main.py").is_file():
        raise SystemExit(f"pycgfx was not found at {pycgfx_root}")

    sys.path.insert(0, str(pycgfx_root))
    import gltflib  # type: ignore[import-not-found]
    import main as pycgfx  # type: ignore[import-not-found]
    from cgfx.mtob import (  # type: ignore[import-not-found]
        ColorFloat,
        FragmentLightingFlags,
    )
    from cgfx.canm import (  # type: ignore[import-not-found]
        CANMBoneTransform,
        PrimitiveType,
    )
    from cgfx.primitives import (  # type: ignore[import-not-found]
        VertexAttributeUsage,
    )
    from cgfx.sobj import BillboardMode  # type: ignore[import-not-found]

    gltf = gltflib.GLTF.load(
        str(args.input_gltf.resolve()), load_file_resources=True
    )
    cgfx = pycgfx.convert_gltf(gltf)

    matched = False
    matte_materials: list[str] = []
    rigid_mesh_nodes: list[str] = []
    for model_name in cgfx.data.models:
        model = cgfx.data.models[model_name]
        skeleton = getattr(model, "skeleton", None)
        if skeleton is None:
            continue
        logo_bone = skeleton.bones["name"]
        if logo_bone is None:
            continue
        logo_bone.billboard_mode = BillboardMode.YAxial
        matched = True

        # pycgfx leaves this runtime lookup field empty.  Nintendo's AR Games
        # banner fills it for every rigid mesh, so explicitly bind each SOBJ
        # mesh to the bone created from its glTF mesh node.
        mesh_node_names = ("name", *sorted(RIGID_ANIMATED_MESHES))
        for mesh in model.meshes.data.contents:
            matched_node = next(
                (
                    node_name
                    for node_name in mesh_node_names
                    if mesh.name == node_name
                    or mesh.name.startswith(node_name + "_")
                ),
                None,
            )
            if matched_node is None and mesh.name.startswith("Plane_"):
                matched_node = "name"
            if matched_node is None:
                raise SystemExit(
                    f"could not resolve mesh node for SOBJ {mesh.name}"
                )
            mesh.mesh_node_name = matched_node
            rigid_mesh_nodes.append(matched_node)

        # A hardware-safe banner must contain no soft-skin primitive sets and
        # no bone-index/weight streams.  Fail the build rather than silently
        # producing another HOME Menu lock-up candidate.
        for shape in model.shapes.data.contents:
            for primitive_set in shape.primitive_sets.data.contents:
                if primitive_set.skinning_mode != 0:
                    raise SystemExit(
                        f"{shape.name} still uses skinning mode "
                        f"{primitive_set.skinning_mode}"
                    )
            for attribute in shape.vertex_attributes.data.contents:
                usages = (
                    attribute.vertex_streams.data.contents
                    if hasattr(attribute, "vertex_streams")
                    else (attribute,)
                )
                for usage in usages:
                    if usage.usage in (
                        VertexAttributeUsage.BoneIndex,
                        VertexAttributeUsage.BoneWeight,
                    ):
                        raise SystemExit(
                            f"{shape.name} still contains skin attributes"
                        )

        # pycgfx's generic PBR conversion adds a very strong white specular
        # term (roughly 0.4-0.55 for these materials).  On the HOME Menu that
        # washes the model and fixed logo nearly white whenever they face the
        # key light.  Keep diffuse lighting for the model's dimensionality,
        # but make every banner material matte by bypassing the specular TEV
        # stage and removing its distribution lookup.
        for material_name in model.materials:
            material = model.materials[material_name]
            material.material_color.constant[0] = ColorFloat(0, 0, 0, 1)
            specular_stage = material.fragment_shader.texture_combiners[2]
            specular_stage.src_rgb = 0xFFF
            specular_stage.combine_rgb = 0
            material.fragment_shader.fragment_lighting.flags = (
                FragmentLightingFlags(0)
            )
            material.fragment_shader.fragment_lighting_table.distribution_0_sampler = (
                None
            )
            matte_materials.append(material_name)

    if not matched:
        raise SystemExit("converted CGFX has no root-level 'name' logo bone")

    skeletal_animation = cgfx.data.skeletal_animations["COMMON"]
    if skeletal_animation is None:
        raise SystemExit("converted CGFX has no COMMON skeletal animation")
    animation_members = {
        member_name: skeletal_animation.member_animations_data[member_name]
        for member_name in skeletal_animation.member_animations_data
    }
    if set(animation_members) != RIGID_ANIMATED_MESHES:
        raise SystemExit(
            "expected only rigid mesh animation members, got "
            + ", ".join(sorted(animation_members))
        )
    for member_name, member in animation_members.items():
        if (
            not isinstance(member, CANMBoneTransform)
            or member.primitive_type != PrimitiveType.Transform
        ):
            raise SystemExit(
                f"{member_name} is not ordinary Transform (5)"
            )

    output_path = args.output_cgfx.resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(pycgfx.write(cgfx))
    print(
        f"wrote {output_path} with name billboard mode YAxial (5); "
        f"matte materials: {', '.join(matte_materials)}; "
        "mesh nodes: "
        f"{', '.join(rigid_mesh_nodes)}; "
        "validated ordinary rigid Transform (5) animation members"
    )


if __name__ == "__main__":
    main()
