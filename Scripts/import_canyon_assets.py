"""Import the three-part 24 m canyon modules into /Game/IslandAssets/CanyonModules."""

import json
import os
import runpy
import unreal

root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "SourceAssets", "CanyonModules"))
with open(os.path.join(root, "CanyonModules_manifest.json"), encoding="utf-8") as source:
    manifest = json.load(source)

filenames = []
for variant in manifest["tile_variants"]:
    filenames.extend((variant["wall_left"], variant["wall_right"], variant["floor"]))
filenames.extend(item["file"] for item in manifest["dressing"])

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
for filename in filenames:
    task = unreal.AssetImportTask()
    task.filename = os.path.join(root, filename)
    task.destination_path = "/Game/IslandAssets/CanyonModules"
    task.automated = True
    task.replace_existing = True
    task.save = True
    asset_tools.import_asset_tasks([task])
    meshes = []
    for object_path in task.imported_object_paths:
        asset = unreal.EditorAssetLibrary.load_asset(object_path)
        if isinstance(asset, unreal.StaticMesh):
            body_setup = asset.get_editor_property("body_setup")
            body_setup.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            unreal.EditorAssetLibrary.save_loaded_asset(asset)
            meshes.append(object_path)
    unreal.log(f"TREASURE_CANYON_IMPORTED {filename}: {meshes}")
    if not meshes:
        unreal.log_error(f"TREASURE_CANYON_IMPORT_FAILED {filename}")

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log("TREASURE_CANYON_IMPORT_COMPLETE")
runpy.run_path(os.path.join(os.path.dirname(__file__), "prepare_canyon_instance_materials.py"))
