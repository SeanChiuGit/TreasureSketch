import bpy
import os
from mathutils import Vector


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PACK = r"C:\Users\seanc\Downloads\KayKit_Adventurers_2.0_FREE\KayKit_Adventurers_2.0_FREE"
if not os.path.isdir(PACK):
    PACK = os.path.join(ROOT, "SourceAssets", "ThirdParty", "KayKit_Adventurers_2.0", "KayKit_Adventurers_2.0_FREE")

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=os.path.join(PACK, "Characters", "fbx", "Mage.fbx"), automatic_bone_orientation=False)

for obj in bpy.context.scene.objects:
    parent = obj.parent.name if obj.parent else "-"
    print("OBJECT", obj.name, obj.type, "PARENT", parent)
    if obj.type == "MESH":
        corners = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
        mins = tuple(round(min(c[i] for c in corners), 4) for i in range(3))
        maxs = tuple(round(max(c[i] for c in corners), 4) for i in range(3))
        print(" BOUNDS", mins, maxs, "MATS", [slot.material.name for slot in obj.material_slots])
    if obj.type == "ARMATURE":
        print(" BONES", ",".join(b.name for b in obj.data.bones))

for asset in ("spellbook_closed.fbx", "spellbook_open.fbx", "wand.fbx"):
    before = set(bpy.context.scene.objects)
    bpy.ops.import_scene.fbx(filepath=os.path.join(PACK, "Assets", "fbx", asset), automatic_bone_orientation=False)
    imported = set(bpy.context.scene.objects) - before
    print("ASSET", asset)
    for obj in imported:
        print(" ", obj.name, obj.type, "LOC", tuple(round(v, 4) for v in obj.location), "ROT", tuple(round(v, 4) for v in obj.rotation_euler), "DIM", tuple(round(v, 4) for v in obj.dimensions))
