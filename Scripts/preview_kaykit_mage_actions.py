import bpy
import os
from mathutils import Vector


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PACK = r"C:\Users\seanc\Downloads\KayKit_Adventurers_2.0_FREE\KayKit_Adventurers_2.0_FREE"
OUT = os.path.join(ROOT, "SourceAssets", "KayKitMageExplorer", "ActionTests")
os.makedirs(OUT, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=os.path.join(PACK, "Characters", "fbx", "Mage.fbx"), automatic_bone_orientation=False)
arm = next(obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE")

for filename in ("Rig_Medium_General.fbx", "Rig_Medium_MovementBasic.fbx"):
    before_objects = set(bpy.context.scene.objects)
    bpy.ops.import_scene.fbx(filepath=os.path.join(PACK, "Animations", "fbx", "Rig_Medium", filename), automatic_bone_orientation=False)
    for obj in set(bpy.context.scene.objects) - before_objects:
        bpy.data.objects.remove(obj, do_unlink=True)

scene = bpy.context.scene
scene.render.engine = "BLENDER_EEVEE"
scene.render.resolution_x = 512
scene.render.resolution_y = 512
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
if scene.world is None:
    scene.world = bpy.data.worlds.new("PreviewWorld")
scene.world.use_nodes = True
scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.08, 0.12, 0.15, 1.0)
scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.4

bpy.ops.mesh.primitive_plane_add(size=100, location=(0, 0, -0.015))
floor = bpy.context.object
mat = bpy.data.materials.new("Ground")
mat.diffuse_color = (0.12, 0.18, 0.18, 1.0)
floor.data.materials.append(mat)

bpy.ops.object.light_add(type="AREA", location=(-3.0, -4.0, 6.0))
bpy.context.object.data.energy = 1300
bpy.context.object.data.size = 4.0
bpy.ops.object.light_add(type="AREA", location=(4.0, 1.0, 3.5))
bpy.context.object.data.energy = 800
bpy.context.object.data.color = (0.4, 0.7, 1.0)
bpy.context.object.data.size = 4.0
bpy.ops.object.camera_add(location=(3.2, -6.7, 2.85))
cam = bpy.context.object
cam.data.lens = 58
cam.rotation_euler = (Vector((0, 0, 1.3)) - cam.location).to_track_quat("-Z", "Y").to_euler()
scene.camera = cam

arm.animation_data_create()
tests = {
    "Idle_A": 17,
    "Running_A": 7,
    "Use_Item": 25,
    "Interact": 20,
    "PickUp": 20,
    "Throw": 21,
}
for suffix, frame in tests.items():
    action = next((item for item in bpy.data.actions if item.name.endswith("|" + suffix)), None)
    if not action:
        print("MISSING", suffix)
        continue
    arm.animation_data.action = action
    if action.slots:
        arm.animation_data.action_slot = action.slots[0]
    scene.frame_set(frame)
    scene.render.filepath = os.path.join(OUT, suffix + ".png")
    bpy.ops.render.render(write_still=True)
    for bone_name in ("upperarm.l", "lowerarm.l", "wrist.l", "upperarm.r", "lowerarm.r", "wrist.r", "chest", "head"):
        bone = arm.pose.bones[bone_name]
        values = tuple(round(v, 3) for v in bone.matrix_basis.to_euler("XYZ"))
        print("POSE", suffix, bone_name, values)
    print("RENDERED", suffix, frame)
