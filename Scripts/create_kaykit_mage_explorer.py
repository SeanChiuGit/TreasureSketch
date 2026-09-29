import bpy
import math
import os
import shutil
from mathutils import Euler, Matrix, Vector


PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
USER_PACK = r"C:\Users\seanc\Downloads\KayKit_Adventurers_2.0_FREE\KayKit_Adventurers_2.0_FREE"
PROJECT_PACK = os.path.join(
    PROJECT_ROOT,
    "SourceAssets",
    "ThirdParty",
    "KayKit_Adventurers_2.0",
    "KayKit_Adventurers_2.0_FREE",
)
PACK_ROOT = USER_PACK if os.path.isdir(USER_PACK) else PROJECT_PACK
OUTPUT_ROOT = os.path.join(PROJECT_ROOT, "SourceAssets", "KayKitMageExplorer")
os.makedirs(OUTPUT_ROOT, exist_ok=True)

MAGE_FBX = os.path.join(PACK_ROOT, "Characters", "fbx", "Mage.fbx")
ASSET_ROOT = os.path.join(PACK_ROOT, "Assets", "fbx")
ANIMATION_ROOT = os.path.join(PACK_ROOT, "Animations", "fbx", "Rig_Medium")


def import_mage():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=MAGE_FBX, automatic_bone_orientation=False)
    return next(obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE")


def import_animation_library(armature):
    loaded = []
    for filename in ("Rig_Medium_General.fbx", "Rig_Medium_MovementBasic.fbx"):
        before_objects = set(bpy.context.scene.objects)
        before_actions = set(bpy.data.actions)
        bpy.ops.import_scene.fbx(
            filepath=os.path.join(ANIMATION_ROOT, filename),
            automatic_bone_orientation=False,
        )
        imported_objects = set(bpy.context.scene.objects) - before_objects
        new_actions = set(bpy.data.actions) - before_actions
        for action in new_actions:
            action.use_fake_user = True
            action.name = "A_MageExplorer_" + action.name.split("|")[-1]
            loaded.append(action)
        for obj in imported_objects:
            bpy.data.objects.remove(obj, do_unlink=True)

    armature.animation_data_create()
    armature.animation_data.action = None
    seen = set()
    for action in loaded:
        action.name = action.name.replace(".001", "")
        if action.name in seen:
            continue
        seen.add(action.name)
        track = armature.animation_data.nla_tracks.new()
        track.name = action.name
        strip = track.strips.new(action.name, int(action.frame_range[0]), action)
        strip.action_frame_start = action.frame_range[0]
        strip.action_frame_end = action.frame_range[1]
        if action.slots:
            strip.action_slot = action.slots[0]
        track.mute = True
    return loaded


def use_action(armature, name, frame):
    action = bpy.data.actions.get(name)
    if not action:
        raise RuntimeError(f"Missing action: {name}")
    armature.animation_data_create()
    armature.animation_data.action = action
    if action.slots:
        armature.animation_data.action_slot = action.slots[0]
    bpy.context.scene.frame_set(frame)
    bpy.context.view_layer.update()
    return action


def capture_pose(armature):
    return {bone.name: bone.matrix_basis.copy() for bone in armature.pose.bones}


def apply_pose(armature, pose):
    for name, matrix in pose.items():
        armature.pose.bones[name].matrix_basis = matrix.copy()


def override_pose(armature, rotations=None, locations=None):
    rotations = rotations or {}
    locations = locations or {}
    for name, values in rotations.items():
        bone = armature.pose.bones[name]
        location, _, scale = bone.matrix_basis.decompose()
        rotation = Euler(values, "XYZ").to_quaternion()
        bone.matrix_basis = Matrix.LocRotScale(location, rotation, scale)
    for name, values in locations.items():
        bone = armature.pose.bones[name]
        _, rotation, scale = bone.matrix_basis.decompose()
        bone.matrix_basis = Matrix.LocRotScale(Vector(values), rotation, scale)


def key_current_pose(armature, frame):
    for bone in armature.pose.bones:
        bone.rotation_mode = "QUATERNION"
        location, rotation, scale = bone.matrix_basis.decompose()
        bone.location = location
        bone.rotation_quaternion = rotation
        bone.scale = scale
        bone.keyframe_insert("location", frame=frame)
        bone.keyframe_insert("rotation_quaternion", frame=frame)
        bone.keyframe_insert("scale", frame=frame)


def create_custom_action(armature, name, keys):
    action = bpy.data.actions.new(name)
    action.use_fake_user = True
    armature.animation_data.action = action
    for frame, base_pose, rotations, locations in keys:
        apply_pose(armature, base_pose)
        override_pose(armature, rotations, locations)
        key_current_pose(armature, frame)
    if action.slots:
        armature.animation_data.action_slot = action.slots[0]

    armature.animation_data.action = None
    track = armature.animation_data.nla_tracks.new()
    track.name = name
    strip = track.strips.new(name, int(action.frame_range[0]), action)
    strip.action_frame_start = action.frame_range[0]
    strip.action_frame_end = action.frame_range[1]
    if action.slots:
        strip.action_slot = action.slots[0]
    track.mute = True
    return action


def build_custom_actions(armature):
    use_action(armature, "A_MageExplorer_Idle_A", 17)
    idle = capture_pose(armature)
    use_action(armature, "A_MageExplorer_Throw", 21)
    raised = capture_pose(armature)
    use_action(armature, "A_MageExplorer_PickUp", 20)
    crouched = capture_pose(armature)

    read_hold = {
        "upperarm.l": (-0.52, -0.78, 0.34),
        "lowerarm.l": (0.0, 0.0, -1.30),
        "wrist.l": (0.0, -0.10, 0.10),
        "upperarm.r": (-0.52, -0.78, -0.34),
        "lowerarm.r": (0.0, 0.0, 1.30),
        "wrist.r": (0.0, 0.10, -0.10),
        "chest": (-0.14, 0.0, 0.0),
        "head": (0.46, 0.0, 0.0),
    }
    read_turn_page = dict(read_hold)
    read_turn_page.update(
        {
            "upperarm.r": (-0.40, -0.62, -0.22),
            "lowerarm.r": (0.0, 0.0, 1.48),
            "wrist.r": (0.0, 0.20, -0.30),
            "head": (0.50, 0.08, -0.04),
        }
    )
    read = create_custom_action(
        armature,
        "A_MageExplorer_ReadBook",
        [
            (1, idle, read_hold, {}),
            (18, idle, read_hold, {}),
            (34, idle, read_turn_page, {}),
            (48, idle, read_hold, {}),
            (60, idle, read_hold, {}),
        ],
    )

    dig_raise = {
        "chest": (-0.14, -0.08, -0.05),
        "head": (-0.05, 0.10, 0.06),
        "upperarm.r": (0.42, 1.05, 0.48),
        "lowerarm.r": (0.0, 0.0, 0.64),
        "wrist.r": (0.0, 0.0, -0.12),
    }
    dig_hit = {
        "chest": (0.24, -0.04, -0.05),
        "head": (0.08, -0.14, -0.02),
        "upperarm.r": (-0.08, 0.12, -0.16),
        "lowerarm.r": (0.0, 0.0, 1.18),
        "wrist.r": (0.0, 0.08, 0.18),
        "upperarm.l": (-0.76, 0.34, 0.08),
        "lowerarm.l": (0.0, 0.0, -0.54),
    }
    dig = create_custom_action(
        armature,
        "A_MageExplorer_WandDig",
        [
            (1, idle, {}, {}),
            (9, raised, dig_raise, {}),
            (18, idle, dig_hit, {}),
            (23, idle, dig_hit, {}),
            (32, idle, {}, {}),
            (36, idle, {}, {}),
        ],
    )
    return read, dig


def import_prop(filename, name):
    before = set(bpy.context.scene.objects)
    bpy.ops.import_scene.fbx(
        filepath=os.path.join(ASSET_ROOT, filename),
        automatic_bone_orientation=False,
    )
    imported = [obj for obj in set(bpy.context.scene.objects) - before if obj.type == "MESH"]
    if not imported:
        raise RuntimeError(f"No mesh imported from {filename}")
    prop = imported[0]
    prop.name = name
    return prop


def bone_parent_preserve_world(obj, armature, bone_name):
    world = obj.matrix_world.copy()
    obj.parent = armature
    obj.parent_type = "BONE"
    obj.parent_bone = bone_name
    obj.matrix_world = world


def prepare_prop(obj, location, rotation, scale):
    obj.location = location
    obj.rotation_euler = rotation
    obj.scale = scale
    bpy.context.view_layer.update()


def build_props(armature):
    use_action(armature, "A_MageExplorer_Idle_A", 17)

    back_book = import_prop("spellbook_closed.fbx", "PROP_Book_Back")
    prepare_prop(back_book, (0.26, 0.60, 1.04), (math.radians(5), math.radians(-18), math.radians(-10)), (0.84, 0.84, 0.84))
    bone_parent_preserve_world(back_book, armature, "chest")
    back_book["default_visible"] = True

    back_staff = import_prop("staff.fbx", "PROP_MagicStaff_Back")
    prepare_prop(back_staff, (-0.24, 0.62, 1.18), (0.0, math.radians(-28), math.radians(7)), (0.82, 0.82, 0.82))
    bone_parent_preserve_world(back_staff, armature, "chest")
    back_staff["default_visible"] = True

    use_action(armature, "A_MageExplorer_ReadBook", 24)
    hand_book = import_prop("spellbook_open.fbx", "PROP_Book_Open")
    prepare_prop(hand_book, (0.0, -0.49, 0.96), (math.radians(-8), 0.0, math.radians(180)), (0.58, 0.58, 0.58))
    bone_parent_preserve_world(hand_book, armature, "chest")
    hand_book["default_visible"] = False
    hand_book["show_during"] = "ReadBook"

    use_action(armature, "A_MageExplorer_WandDig", 18)
    hand_staff = import_prop("staff.fbx", "PROP_MagicStaff_Hand")
    prepare_prop(hand_staff, (-0.60, -0.38, 0.61), (0.0, math.radians(8), math.radians(4)), (0.76, 0.76, 0.76))
    bone_parent_preserve_world(hand_staff, armature, "handslot.r")
    hand_staff["default_visible"] = False
    hand_staff["show_during"] = "WandDig"

    return {
        "back_book": back_book,
        "back_wand": back_staff,
        "hand_book": hand_book,
        "hand_wand": hand_staff,
    }


def setup_preview_scene():
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 640
    scene.render.resolution_y = 640
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.film_transparent = False
    scene.view_settings.look = "AgX - Medium High Contrast"
    if scene.world is None:
        scene.world = bpy.data.worlds.new("MageExplorerWorld")
    scene.world.use_nodes = True
    background = scene.world.node_tree.nodes["Background"]
    background.inputs["Color"].default_value = (0.075, 0.105, 0.16, 1.0)
    background.inputs["Strength"].default_value = 0.40

    ground = bpy.data.materials.new("M_MageExplorer_Ground")
    ground.diffuse_color = (0.11, 0.16, 0.22, 1.0)
    bpy.ops.mesh.primitive_plane_add(size=100, location=(0, 0, -0.015))
    bpy.context.object.name = "PreviewGround"
    bpy.context.object.data.materials.append(ground)

    bpy.ops.object.light_add(type="AREA", location=(-3.2, -4.3, 6.2))
    key = bpy.context.object
    key.data.energy = 1250
    key.data.size = 4.4
    bpy.ops.object.light_add(type="AREA", location=(4.0, 1.0, 3.7))
    fill = bpy.context.object
    fill.data.energy = 850
    fill.data.color = (0.35, 0.62, 1.0)
    fill.data.size = 4.0
    bpy.ops.object.light_add(type="AREA", location=(-2.0, 3.4, 4.8))
    rim = bpy.context.object
    rim.data.energy = 700
    rim.data.color = (0.92, 0.30, 0.72)
    rim.data.size = 3.2

    bpy.ops.object.camera_add(location=(3.15, -6.9, 3.0))
    camera = bpy.context.object
    camera.name = "PreviewCamera"
    camera.data.lens = 60
    camera.rotation_euler = (Vector((0.0, 0.0, 1.32)) - camera.location).to_track_quat("-Z", "Y").to_euler()
    scene.camera = camera


def set_prop_state(props, state):
    if state == "default":
        visibility = (True, True, False, False)
    elif state == "read":
        visibility = (False, True, True, False)
    elif state == "dig":
        visibility = (True, False, False, True)
    else:
        raise ValueError(state)
    for obj, visible in zip(
        (props["back_book"], props["back_wand"], props["hand_book"], props["hand_wand"]),
        visibility,
    ):
        obj.hide_render = not visible
        obj.hide_viewport = not visible


def render_still(armature, props, action_name, frame, state, filename):
    set_prop_state(props, state)
    use_action(armature, action_name, frame)
    scene = bpy.context.scene
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = os.path.join(OUTPUT_ROOT, filename)
    bpy.ops.render.render(write_still=True)


def render_back_still(armature, props, filename):
    set_prop_state(props, "default")
    use_action(armature, "A_MageExplorer_Idle_A", 17)
    scene = bpy.context.scene
    camera = bpy.data.objects["PreviewCamera"]
    old_location = camera.location.copy()
    old_rotation = camera.rotation_euler.copy()
    camera.location = (-3.15, 6.9, 3.0)
    camera.rotation_euler = (Vector((0.0, 0.0, 1.32)) - camera.location).to_track_quat("-Z", "Y").to_euler()
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = os.path.join(OUTPUT_ROOT, filename)
    bpy.ops.render.render(write_still=True)
    camera.location = old_location
    camera.rotation_euler = old_rotation


def render_sequence(armature, props, action_name, frame_start, frame_end, state, folder_name):
    set_prop_state(props, state)
    use_action(armature, action_name, frame_start)
    scene = bpy.context.scene
    scene.frame_start = frame_start
    scene.frame_end = frame_end
    scene.render.resolution_x = 512
    scene.render.resolution_y = 512
    scene.render.image_settings.file_format = "PNG"
    frame_root = os.path.join(OUTPUT_ROOT, "PreviewFrames", folder_name)
    os.makedirs(frame_root, exist_ok=True)
    scene.render.filepath = os.path.join(frame_root, "Frame_")
    bpy.ops.render.render(animation=True)
    scene.render.resolution_x = 640
    scene.render.resolution_y = 640


def export_assets(armature, props):
    set_prop_state(props, "default")
    scene = bpy.context.scene
    armature.animation_data.action = None
    scene.frame_set(1)
    bpy.ops.object.select_all(action="DESELECT")
    armature.select_set(True)
    for obj in scene.objects:
        # Runtime gear is imported as separate Unreal static meshes and attached
        # to bones/components. Export only the skinned Mage body here so the
        # back and hand variants are not visible at the same time in game.
        if obj.type == "MESH" and obj.name.startswith("Mage_"):
            obj.hide_viewport = False
            obj.hide_render = False
            obj.select_set(True)
    bpy.context.view_layer.objects.active = armature

    bpy.ops.export_scene.gltf(
        filepath=os.path.join(OUTPUT_ROOT, "SK_KayKitMageExplorer.glb"),
        export_format="GLB",
        use_selection=True,
        export_animations=True,
        export_nla_strips=True,
        export_def_bones=True,
    )
    bpy.ops.export_scene.fbx(
        filepath=os.path.join(OUTPUT_ROOT, "SK_KayKitMageExplorer.fbx"),
        use_selection=True,
        add_leaf_bones=False,
        apply_unit_scale=True,
        bake_anim=True,
        bake_anim_use_all_actions=True,
        bake_anim_use_nla_strips=False,
        object_types={"ARMATURE", "MESH"},
        axis_forward="-Y",
        axis_up="Z",
        path_mode="COPY",
        embed_textures=True,
    )


armature = import_mage()
import_animation_library(armature)
build_custom_actions(armature)
props = build_props(armature)
setup_preview_scene()

render_still(armature, props, "A_MageExplorer_Idle_A", 17, "default", "MageExplorer_Default.png")
render_back_still(armature, props, "MageExplorer_DefaultBack.png")
render_still(armature, props, "A_MageExplorer_ReadBook", 24, "read", "MageExplorer_ReadBook.png")
render_still(armature, props, "A_MageExplorer_WandDig", 18, "dig", "MageExplorer_WandDig.png")
render_still(armature, props, "A_MageExplorer_Running_A", 7, "default", "MageExplorer_Run.png")

render_sequence(armature, props, "A_MageExplorer_ReadBook", 1, 60, "read", "ReadBook")
render_sequence(armature, props, "A_MageExplorer_WandDig", 1, 36, "dig", "WandDig")
render_sequence(armature, props, "A_MageExplorer_Running_A", 1, 25, "default", "Run")

export_assets(armature, props)
set_prop_state(props, "default")
use_action(armature, "A_MageExplorer_Idle_A", 17)
for image in bpy.data.images:
    if image.source == "FILE":
        try:
            image.pack()
        except RuntimeError:
            pass
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUTPUT_ROOT, "KayKitMageExplorer_Rigged.blend"))
shutil.copy2(os.path.join(PACK_ROOT, "License.txt"), os.path.join(OUTPUT_ROOT, "KayKit_License.txt"))
print("KAYKIT_MAGE_EXPLORER_COMPLETE", OUTPUT_ROOT)
