import os
import unreal


PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
CHARACTER_SOURCE = os.path.join(
    PROJECT_ROOT, "SourceAssets", "KayKitMageExplorer", "SK_KayKitMageExplorer.fbx")
PACK_ROOT = os.path.join(
    PROJECT_ROOT,
    "SourceAssets",
    "ThirdParty",
    "KayKit_Adventurers_2.0",
    "KayKit_Adventurers_2.0_FREE",
)
if not os.path.isdir(PACK_ROOT):
    PACK_ROOT = r"C:\Users\seanc\Downloads\KayKit_Adventurers_2.0_FREE\KayKit_Adventurers_2.0_FREE"

DEST = "/Game/Characters/KayKitMageExplorer"


def import_character():
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", True)
    options.set_editor_property("import_animations", True)
    options.set_editor_property("import_materials", True)
    options.set_editor_property("import_textures", True)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    options.skeletal_mesh_import_data.set_editor_property("import_mesh_lo_ds", False)
    options.skeletal_mesh_import_data.set_editor_property("update_skeleton_reference_pose", False)
    options.anim_sequence_import_data.set_editor_property(
        "animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", CHARACTER_SOURCE)
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("destination_name", "SK_KayKitMageExplorer")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])


def import_static_prop(filename, destination_name):
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.static_mesh_import_data.set_editor_property("combine_meshes", True)
    options.static_mesh_import_data.set_editor_property("generate_lightmap_u_vs", False)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.join(
        PROJECT_ROOT, "SourceAssets", "KayKitMageExplorer", "Props", filename))
    task.set_editor_property("destination_path", DEST + "/Props")
    task.set_editor_property("destination_name", destination_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])


import_character()
import_static_prop("SM_Spellbook_Closed.fbx", "SM_Spellbook_Closed")
import_static_prop("SM_Spellbook_Open.fbx", "SM_Spellbook_Open")
import_static_prop("SM_MagicStaff.fbx", "SM_MagicStaff")

mage_material = unreal.load_asset(DEST + "/mage")
if mage_material:
    mage_material.set_editor_property("used_with_skeletal_mesh", True)
    unreal.EditorAssetLibrary.save_loaded_asset(mage_material)
    for prop_name in ("SM_Spellbook_Closed", "SM_Spellbook_Open", "SM_MagicStaff"):
        prop = unreal.load_asset(DEST + "/Props/" + prop_name)
        if prop:
            prop.set_material(0, mage_material)
            unreal.EditorAssetLibrary.save_loaded_asset(prop)

assets = unreal.EditorAssetLibrary.list_assets(DEST, recursive=True, include_folder=False)
for path in sorted(assets):
    unreal.log("KAYKIT_MAGE_ASSET " + path)

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log("KAYKIT_MAGE_IMPORT_COMPLETE " + CHARACTER_SOURCE)
