import os

import bpy


PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PACK_ROOT = os.path.join(
    PROJECT_ROOT,
    "SourceAssets",
    "ThirdParty",
    "KayKit_Adventurers_2.0",
    "KayKit_Adventurers_2.0_FREE",
)
if not os.path.isdir(PACK_ROOT):
    PACK_ROOT = r"C:\Users\seanc\Downloads\KayKit_Adventurers_2.0_FREE\KayKit_Adventurers_2.0_FREE"

SOURCE_ROOT = os.path.join(PACK_ROOT, "Assets", "fbx")
OUTPUT_ROOT = os.path.join(PROJECT_ROOT, "SourceAssets", "KayKitMageExplorer", "Props")
os.makedirs(OUTPUT_ROOT, exist_ok=True)


def export_prop(source_name, output_name):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(
        filepath=os.path.join(SOURCE_ROOT, source_name),
        automatic_bone_orientation=False,
    )
    meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    if not meshes:
        raise RuntimeError(f"No mesh imported from {source_name}")
    bpy.ops.object.select_all(action="DESELECT")
    for mesh in meshes:
        mesh.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    output = os.path.join(OUTPUT_ROOT, output_name)
    bpy.ops.export_scene.fbx(
        filepath=output,
        use_selection=True,
        object_types={"MESH"},
        apply_unit_scale=True,
        bake_anim=False,
        axis_forward="-Y",
        axis_up="Z",
        path_mode="COPY",
        embed_textures=True,
    )
    print("KAYKIT_PROP_EXPORT", output)


export_prop("spellbook_closed.fbx", "SM_Spellbook_Closed.fbx")
export_prop("spellbook_open.fbx", "SM_Spellbook_Open.fbx")
export_prop("staff.fbx", "SM_MagicStaff.fbx")
