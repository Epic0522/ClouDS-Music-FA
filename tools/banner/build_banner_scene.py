"""Assemble the animated ClouDS Music HOME Menu banner scene.

Run through Blender:

    blender --background --factory-startup \
      --python tools/banner/build_banner_scene.py -- \
      banner_3d/source/clouds-banner-rhythm.gltf \
      banner_3d/logo-128x64.png \
      banner_3d/banner-scene.gltf

Then preserve the SM64-style Y-axis billboard metadata during conversion:

    .tools/pycgfx/.venv/bin/python \
      tools/banner/convert_banner_cgfx.py \
      banner_3d/banner-scene.gltf banner_3d/banner.cgfx

The original character uses deforming skin weights.  The real HOME Menu hangs
when that skin is animated even though Azahar renders it.  Nintendo's AR Games
banner instead animates rigid mesh nodes with ordinary Transform (5) CANM
members.  This assembler therefore bakes the first skinned pose into the full
resolution meshes, then transfers the authored rhythm motion to the three mesh
objects as rigid transforms.  No decimation and no skin attributes survive.

The logo is exported as the separate ``COMMON/name`` screen plane used by the
proven SM64 banner workflow.  Its complete transform is baked into its vertices
so ``convert_banner_cgfx.py`` can apply CGFX Y-axis billboarding to an identity
bone without producing an edge-on plane.
"""

from __future__ import annotations

import math
import json
import sys
from pathlib import Path

import bpy
from mathutils import Matrix


# Keep the complete animated character + lower logo group inside the HOME
# Menu's practical safe area, including the camera / L+R strip at the bottom.
MODEL_SCALE = 2.85
MODEL_VERTICAL_OFFSET = 3.15
LOGO_WIDTH = 4.4
LOGO_HEIGHT = 2.2
LOGO_DEPTH = -0.82
LOGO_VERTICAL_POSITION = -3.0
SOURCE_RIG = "ClouDS_Rhythm_Rig"
SOURCE_ANIMATED_BONE = "UpperSway"
RIGID_ANIMATED_MESHES = (
    "Cloud_Textured",
    "Front_Decorations",
    "Headphones",
)
RIGID_ANIMATION_PATHS = {"translation", "rotation", "scale"}
SOURCE_LOOP_END_FRAME = 36


def script_args() -> tuple[Path, Path, Path]:
    try:
        separator = sys.argv.index("--")
        args = sys.argv[separator + 1 :]
    except ValueError:
        args = []
    if len(args) != 3:
        raise SystemExit(
            "expected SOURCE_GLTF LOGO_PNG OUTPUT_GLTF after --"
        )
    return tuple(Path(arg).resolve() for arg in args)


def clear_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)


def configure_animation_timing() -> None:
    # The accepted source animation was authored at 30 FPS.  Blender otherwise
    # imports its 1.2-second action into the default 24 FPS scene and re-exports
    # only frames 1..28, which silently removes the final stationary hold.
    scene = bpy.context.scene
    scene.render.fps = 30
    scene.render.fps_base = 1.0
    scene.frame_start = 1
    # Blender keeps its default frame 250 unless told otherwise, even though
    # the imported action ends at 36.  Exporting that unused tail made the
    # HOME Menu wait more than seven seconds between each 1.2-second motion.
    scene.frame_end = SOURCE_LOOP_END_FRAME


def _bone_skin_delta_world(
    rig: bpy.types.Object,
    bone_name: str,
) -> Matrix:
    pose_bone = rig.pose.bones[bone_name]
    rest_bone = rig.data.bones[bone_name]
    return (
        rig.matrix_world
        @ pose_bone.matrix
        @ rest_bone.matrix_local.inverted()
        @ rig.matrix_world.inverted()
    )


def convert_skinned_character_to_rigid_animation() -> None:
    """Bake the authored skin once and animate full-resolution rigid meshes."""

    scene = bpy.context.scene
    rig = bpy.data.objects.get(SOURCE_RIG)
    world = bpy.data.objects.get("world")
    if rig is None or rig.type != "ARMATURE":
        raise RuntimeError(f"missing source armature {SOURCE_RIG}")
    if world is None:
        raise RuntimeError("source scene is missing the expected world node")
    if SOURCE_ANIMATED_BONE not in rig.pose.bones:
        raise RuntimeError(
            f"missing source animation bone {SOURCE_ANIMATED_BONE}"
        )

    meshes = []
    for object_name in RIGID_ANIMATED_MESHES:
        obj = bpy.data.objects.get(object_name)
        if obj is None or obj.type != "MESH":
            raise RuntimeError(f"missing banner mesh {object_name}")
        if not any(modifier.type == "ARMATURE" for modifier in obj.modifiers):
            raise RuntimeError(f"{object_name} has no Armature modifier")
        meshes.append(obj)

    # Capture the exact deformation transform for every authored frame before
    # deleting the rig.  Relative-to-first-frame matrices let us bake whatever
    # rest pose the source uses without applying it twice.
    frame_samples: list[tuple[int, Matrix]] = []
    for frame in range(scene.frame_start, scene.frame_end + 1):
        scene.frame_set(frame)
        frame_samples.append(
            (
                frame,
                _bone_skin_delta_world(
                    rig,
                    SOURCE_ANIMATED_BONE,
                ).copy(),
            )
        )
    first_delta_inverse = frame_samples[0][1].inverted()
    relative_samples = [
        (frame, delta @ first_delta_inverse)
        for frame, delta in frame_samples
    ]

    scene.frame_set(scene.frame_start)
    for obj in meshes:
        # Applying at the first frame preserves all original triangles, UVs,
        # normals and the accepted rest silhouette.  Clearing the armature
        # parent afterwards removes both the skin and its JOINTS/WEIGHTS
        # streams from the exported glTF.
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        for modifier in tuple(obj.modifiers):
            if modifier.type == "ARMATURE":
                bpy.ops.object.modifier_apply(modifier=modifier.name)

        # Keep the mesh parented to ``world``.  That node receives the final
        # HOME Menu safe-area scale/offset below, and detaching the meshes here
        # would silently leave the animated character outside that transform.
        first_world = obj.matrix_world.copy()
        obj.parent = world
        obj.matrix_world = first_world
        obj.rotation_mode = "QUATERNION"
        obj.animation_data_clear()

        for frame, relative_delta in relative_samples:
            obj.matrix_world = relative_delta @ first_world
            obj.keyframe_insert("location", frame=frame)
            obj.keyframe_insert("rotation_quaternion", frame=frame)
            obj.keyframe_insert("scale", frame=frame)

        for group in tuple(obj.vertex_groups):
            obj.vertex_groups.remove(group)
        obj.select_set(False)

    # Meshes now own independent rigid actions.  The source armature and its
    # deform bones must not reach glTF/CGFX at all.
    bpy.data.objects.remove(rig, do_unlink=True)
    scene.frame_set(scene.frame_start)


def create_common_root() -> bpy.types.Object:
    common = bpy.data.objects.new("COMMON", None)
    bpy.context.scene.collection.objects.link(common)
    return common


def create_logo_plane(logo_path: Path) -> bpy.types.Object:
    # Preserve the exact visual placement from the former child-of-world
    # layout, but bake that parent's uniform scale and translation directly
    # into this independent screen plane.
    logo_location = (
        0.0,
        LOGO_DEPTH * MODEL_SCALE,
        MODEL_VERTICAL_OFFSET + LOGO_VERTICAL_POSITION * MODEL_SCALE,
    )
    bpy.ops.mesh.primitive_plane_add(
        size=2.0,
        location=logo_location,
        # The HOME Menu starts with the camera on +Y.  Face the textured side
        # toward it so the logo remains readable instead of showing the
        # mirrored back face of the alpha-cutout plane.
        rotation=(math.radians(-90.0), 0.0, 0.0),
    )
    logo = bpy.context.object
    # ``name`` is the established fixed screen-plane node from the working
    # SM64 3D-banner pipeline.  It must remain a root-level sibling of
    # ``world`` rather than becoming part of the animated model hierarchy.
    logo.name = "name"
    logo.scale = (
        LOGO_WIDTH * MODEL_SCALE * 0.5,
        LOGO_HEIGHT * MODEL_SCALE * 0.5,
        1.0,
    )
    # Reversing the plane normal for the HOME Menu camera also reverses its
    # vertical texture orientation.  Flip only V so the front face is both
    # readable and right-side-up.
    uv_layer = logo.data.uv_layers.active
    if uv_layer is None:
        raise RuntimeError("generated logo plane has no UV layer")
    for loop in uv_layer.data:
        loop.uv.y = 1.0 - loop.uv.y

    material = bpy.data.materials.new("ClouDS_Music_Logo_Mask")
    material.use_nodes = True
    material.diffuse_color = (1.0, 1.0, 1.0, 1.0)
    material.use_backface_culling = False
    if hasattr(material, "surface_render_method"):
        material.surface_render_method = "DITHERED"
    if hasattr(material, "alpha_threshold"):
        material.alpha_threshold = 0.5

    nodes = material.node_tree.nodes
    links = material.node_tree.links
    principled = nodes.get("Principled BSDF")
    texture = nodes.new("ShaderNodeTexImage")
    texture.name = "ClouDS Music Logo"
    texture.image = bpy.data.images.load(str(logo_path), check_existing=True)
    texture.interpolation = "Linear"
    links.new(texture.outputs["Color"], principled.inputs["Base Color"])
    links.new(texture.outputs["Alpha"], principled.inputs["Alpha"])
    principled.inputs["Metallic"].default_value = 0.0
    principled.inputs["Roughness"].default_value = 0.65
    logo.data.materials.append(material)

    # The SM64 banner's working ``name`` bone is identity-transformed.  Bake
    # our visual transform into the quad so CGFX YAxial billboarding operates
    # on the expected XY card rather than applying over Blender's -90° node
    # rotation and turning it edge-on.
    bpy.context.view_layer.objects.active = logo
    logo.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    logo.select_set(False)
    return logo


def export_scene(output_path: Path) -> None:
    bpy.ops.export_scene.gltf(
        filepath=str(output_path),
        export_format="GLTF_SEPARATE",
        export_animations=True,
        export_animation_mode="ACTIVE_ACTIONS",
        export_nla_strips_merged_animation_name="ClouDS_Rhythm_Idle",
        export_frame_range=True,
        export_frame_step=1,
        export_force_sampling=True,
        export_skins=True,
        export_cameras=False,
        export_lights=False,
    )
    document = json.loads(output_path.read_text(encoding="utf-8"))
    for material in document.get("materials", []):
        if material.get("name") == "ClouDS_Music_Logo_Mask":
            material["alphaMode"] = "MASK"
            material["alphaCutoff"] = 0.5
            material["doubleSided"] = True
            break
    else:
        raise RuntimeError("exported scene is missing the logo material")

    # Keep only the three rigid mesh-node transforms.  Nintendo's AR Games
    # banner uses Transform (5) CANM members targeting its individual mesh
    # nodes, so this deliberately mirrors that hardware-proven layout.
    for animation in document.get("animations", []):
        old_samplers = animation.get("samplers", [])
        kept_channels = []
        kept_samplers = []
        for channel in animation.get("channels", []):
            target = channel.get("target", {})
            node_index = target.get("node")
            node_name = (
                document["nodes"][node_index].get("name")
                if isinstance(node_index, int)
                else None
            )
            if (
                node_name not in RIGID_ANIMATED_MESHES
                or target.get("path") not in RIGID_ANIMATION_PATHS
            ):
                continue
            sampler = old_samplers[channel["sampler"]]
            if sampler.get("interpolation", "LINEAR") != "LINEAR":
                raise RuntimeError(
                    f"{node_name} {target.get('path')} is not LINEAR"
                )
            channel["sampler"] = len(kept_samplers)
            kept_samplers.append(sampler)
            kept_channels.append(channel)
        expected_channel_count = (
            len(RIGID_ANIMATED_MESHES) * len(RIGID_ANIMATION_PATHS)
        )
        if len(kept_channels) != expected_channel_count:
            raise RuntimeError(
                "expected exactly three transform tracks for each rigid mesh, "
                f"got {len(kept_channels)} of {expected_channel_count}"
            )
        animation["channels"] = kept_channels
        animation["samplers"] = kept_samplers

    output_path.write_text(
        json.dumps(document, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )


def main() -> None:
    source_path, logo_path, output_path = script_args()
    output_path.parent.mkdir(parents=True, exist_ok=True)

    clear_scene()
    configure_animation_timing()
    bpy.ops.import_scene.gltf(filepath=str(source_path))
    convert_skinned_character_to_rigid_animation()

    root = bpy.data.objects.get("world")
    if root is None:
        raise RuntimeError("source scene is missing the expected world node")

    common = create_common_root()
    root.parent = common
    logo = create_logo_plane(logo_path)
    logo.parent = common
    root.scale = (MODEL_SCALE, MODEL_SCALE, MODEL_SCALE)
    root.location = (0.0, 0.0, MODEL_VERTICAL_OFFSET)

    export_scene(output_path)


if __name__ == "__main__":
    main()
