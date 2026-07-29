"""Create a subtle, loopable rhythm animation for the ClouDS 3D banner.

Run through Blender:

    blender --background --factory-startup \
      --python tools/banner/create_rhythm_animation.py -- \
      INPUT.gltf OUTPUT_DIR

The source bundle is never modified. The script writes an editable .blend,
an animated glTF bundle, and a small MP4 preview.
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector


FPS = 30
ACTIVE_END = 28
LOOP_END = 36
MESH_NAMES = ("Cloud_Textured", "Headphones", "Front_Decorations")


def script_args() -> tuple[Path, Path]:
    try:
        separator = sys.argv.index("--")
        args = sys.argv[separator + 1 :]
    except ValueError:
        args = []

    if len(args) != 2:
        raise SystemExit("expected INPUT.gltf and OUTPUT_DIR after --")
    return Path(args[0]).resolve(), Path(args[1]).resolve()


def clear_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)


def add_key(
    obj: bpy.types.Object,
    frame: int,
    *,
    location: tuple[float, float, float] | None = None,
    scale: tuple[float, float, float] | None = None,
    rotation: tuple[float, float, float] | None = None,
) -> None:
    if location is not None:
        obj.location = location
        obj.keyframe_insert("location", frame=frame)
    if scale is not None:
        obj.scale = scale
        obj.keyframe_insert("scale", frame=frame)
    if rotation is not None:
        obj.rotation_mode = "XYZ"
        obj.rotation_euler = tuple(math.radians(value) for value in rotation)
        obj.keyframe_insert("rotation_euler", frame=frame)


def set_bezier_handles(obj: bpy.types.Object) -> None:
    action = obj.animation_data.action if obj.animation_data else None
    if action is None:
        return
    # Blender 5 stores curves in layered channel bags and keyframe insertion
    # already creates Bezier curves. Older Blender versions expose fcurves
    # directly, where explicitly clamping the handles remains useful.
    for fcurve in getattr(action, "fcurves", ()):
        for point in fcurve.keyframe_points:
            point.interpolation = "BEZIER"
            point.handle_left_type = "AUTO_CLAMPED"
            point.handle_right_type = "AUTO_CLAMPED"


def smoothstep(value: float) -> float:
    value = max(0.0, min(1.0, value))
    return value * value * (3.0 - 2.0 * value)


def create_seated_rig(
    root: bpy.types.Object,
    meshes: list[bpy.types.Object],
) -> bpy.types.Object:
    minimum, maximum = scene_bounds(meshes)
    height = maximum.z - minimum.z
    pivot_z = minimum.z + height * 0.27
    full_sway_z = minimum.z + height * 0.58

    armature_data = bpy.data.armatures.new("ClouDS_Rhythm_Rig")
    armature = bpy.data.objects.new("ClouDS_Rhythm_Rig", armature_data)
    bpy.context.scene.collection.objects.link(armature)
    armature.parent = root
    armature.show_in_front = True

    bpy.context.view_layer.objects.active = armature
    armature.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")

    fixed = armature_data.edit_bones.new("RootFixed")
    fixed.head = (0.0, 0.0, minimum.z)
    fixed.tail = (0.0, 0.0, pivot_z)

    upper = armature_data.edit_bones.new("UpperSway")
    upper.head = fixed.tail
    upper.tail = (0.0, 0.0, maximum.z + height * 0.08)
    upper.parent = fixed
    upper.use_connect = True

    bpy.ops.object.mode_set(mode="OBJECT")

    for obj in meshes:
        obj.vertex_groups.clear()
        fixed_group = obj.vertex_groups.new(name="RootFixed")
        upper_group = obj.vertex_groups.new(name="UpperSway")

        for vertex in obj.data.vertices:
            if obj.name == "Headphones":
                upper_weight = 1.0
            else:
                upper_weight = smoothstep(
                    (vertex.co.z - pivot_z) / (full_sway_z - pivot_z)
                )
            fixed_weight = 1.0 - upper_weight
            if fixed_weight > 0.0:
                fixed_group.add([vertex.index], fixed_weight, "REPLACE")
            if upper_weight > 0.0:
                upper_group.add([vertex.index], upper_weight, "REPLACE")

        modifier = obj.modifiers.new(name="ClouDS Rhythm Rig", type="ARMATURE")
        modifier.object = armature
        modifier.use_vertex_groups = True

    return armature


def animate_upper_body(armature: bpy.types.Object) -> None:
    upper = armature.pose.bones["UpperSway"]
    upper.rotation_mode = "XYZ"
    # The fixed root is never keyed. Only the upper body changes weight from
    # one side to the other while compressing and extending from the waist,
    # like a seated character moving with the beat.
    poses = (
        # frame, roll degrees, vertical stretch, lateral breadth
        (0, 0.00, 1.000, 1.000),
        (4, -0.30, 0.996, 1.002),
        (8, -2.35, 0.980, 1.009),
        (11, -3.20, 0.970, 1.013),
        (14, -1.20, 1.011, 0.996),
        (17, 2.15, 1.019, 0.993),
        (20, 3.00, 1.007, 0.998),
        (23, 0.85, 0.996, 1.002),
        (26, -0.38, 1.003, 0.999),
        (28, 0.00, 1.000, 1.000),
        (36, 0.00, 1.000, 1.000),
    )
    for frame, roll, stretch, breadth in poses:
        upper.rotation_euler = (0.0, 0.0, math.radians(roll))
        # A Blender bone's local Y axis runs along its length.
        upper.scale = (breadth, stretch, breadth)
        upper.keyframe_insert("rotation_euler", frame=frame)
        upper.keyframe_insert("scale", frame=frame)
    set_bezier_handles(armature)


def flip_cloud_texture_vertically() -> None:
    cloud = bpy.data.objects["Cloud_Textured"]
    if cloud.type != "MESH" or not cloud.data.uv_layers:
        raise RuntimeError("Cloud_Textured has no UV layer to correct")
    uv_layer = cloud.data.uv_layers.active
    for loop in uv_layer.data:
        loop.uv.y = 1.0 - loop.uv.y


def scene_bounds(objects: list[bpy.types.Object]) -> tuple[Vector, Vector]:
    points = [obj.matrix_world @ Vector(corner) for obj in objects for corner in obj.bound_box]
    minimum = Vector(tuple(min(point[i] for point in points) for i in range(3)))
    maximum = Vector(tuple(max(point[i] for point in points) for i in range(3)))
    return minimum, maximum


def add_preview_camera(objects: list[bpy.types.Object]) -> None:
    minimum, maximum = scene_bounds(objects)
    centre = (minimum + maximum) * 0.5
    width = maximum.x - minimum.x
    height = maximum.z - minimum.z

    camera_data = bpy.data.cameras.new("PreviewCamera")
    camera = bpy.data.objects.new("PreviewCamera", camera_data)
    bpy.context.scene.collection.objects.link(camera)
    camera.location = (centre.x, minimum.y - 9.0, centre.z)
    camera.rotation_euler = (math.radians(90), 0.0, 0.0)
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = max(width * 1.15, height * 1.25)
    bpy.context.scene.camera = camera

    key_data = bpy.data.lights.new("PreviewKey", type="AREA")
    key_data.energy = 800
    key_data.shape = "DISK"
    key_data.size = 5.0
    key = bpy.data.objects.new("PreviewKey", key_data)
    bpy.context.scene.collection.objects.link(key)
    key.location = (centre.x - 2.0, minimum.y - 4.0, centre.z + 4.0)
    key.rotation_euler = (math.radians(55), 0.0, math.radians(-20))

    fill_data = bpy.data.lights.new("PreviewFill", type="AREA")
    fill_data.energy = 450
    fill_data.size = 4.0
    fill = bpy.data.objects.new("PreviewFill", fill_data)
    bpy.context.scene.collection.objects.link(fill)
    fill.location = (centre.x + 3.0, minimum.y - 3.0, centre.z + 1.0)
    fill.rotation_euler = (math.radians(65), 0.0, math.radians(25))


def configure_preview(output_dir: Path) -> None:
    scene = bpy.context.scene
    scene.frame_start = 0
    scene.frame_end = LOOP_END
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 512
    scene.render.resolution_y = 384
    scene.render.resolution_percentage = 100
    preview_frames = output_dir / "preview-frames"
    preview_frames.mkdir(parents=True, exist_ok=True)
    for old_frame in preview_frames.glob("frame-*.png"):
        old_frame.unlink()
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(preview_frames / "frame-")
    scene.render.film_transparent = False
    scene.world.color = (0.055, 0.07, 0.085)
    scene.render.fps = FPS


def export_assets(output_dir: Path) -> None:
    # Keep the editable checkpoint self-contained even if the original desktop
    # glTF bundle is later moved or removed.
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(output_dir / "clouds-banner-rhythm.blend"))

    export_path = output_dir / "clouds-banner-rhythm.gltf"
    bpy.ops.export_scene.gltf(
        filepath=str(export_path),
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


def main() -> None:
    input_path, output_dir = script_args()
    output_dir.mkdir(parents=True, exist_ok=True)

    clear_scene()
    bpy.ops.import_scene.gltf(filepath=str(input_path))

    root = bpy.data.objects.get("world")
    meshes = [bpy.data.objects.get(name) for name in MESH_NAMES]
    if root is None or any(obj is None for obj in meshes):
        raise RuntimeError("unexpected model hierarchy; expected world and the three named meshes")
    mesh_objects = [obj for obj in meshes if obj is not None]

    flip_cloud_texture_vertically()
    armature = create_seated_rig(root, mesh_objects)
    animate_upper_body(armature)

    add_preview_camera(mesh_objects)
    configure_preview(output_dir)
    export_assets(output_dir)
    # The export includes the duplicate closing pose at the loop end. Do not
    # render that duplicate into the looping preview or it would pause a frame.
    bpy.context.scene.frame_end = LOOP_END - 1
    bpy.ops.render.render(animation=True)


if __name__ == "__main__":
    main()
