import os
import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "SourceAssets"))
GROUPS = {
    "JungleRuins": (
        "SM_RuinExplorerHut_A.glb", "SM_RuinTwoLevelTemple_A.glb", "SM_RuinWatchtower_A.glb",
        "SM_RuinShrineHall_A.glb", "SM_RuinCryptEntrance_A.glb",
    ),
    "JungleNature": (
        "SM_ButtressTree_A.glb", "SM_ForkedJungleTree_A.glb", "SM_FernCluster_A.glb",
        "SM_JungleBush_A.glb", "SM_FallenJungleLog_A.glb",
    ),
}

for group, files in GROUPS.items():
    for filename in files:
        task = unreal.AssetImportTask()
        task.filename = os.path.join(ROOT, group, filename)
        task.destination_path = f"/Game/IslandAssets/{group}"
        task.automated = True
        task.replace_existing = True
        task.save = True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        if group == "JungleRuins":
            for object_path in task.imported_object_paths:
                asset = unreal.EditorAssetLibrary.load_asset(object_path)
                if isinstance(asset, unreal.StaticMesh):
                    body_setup = asset.get_editor_property("body_setup")
                    body_setup.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
                    unreal.EditorAssetLibrary.save_loaded_asset(asset)
        unreal.log(f"IMPORTED_JUNGLE_THEME {filename}: {task.imported_object_paths}")

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log("TREASURE_JUNGLE_THEME_IMPORT_COMPLETE")
