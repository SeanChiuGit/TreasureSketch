import os
import unreal

SOURCE = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "SourceAssets", "PirateLandmarks"))
DEST = "/Game/IslandAssets/PirateLandmarks"

FILES = (
    "SM_SkullIdol_A.glb",
    "SM_FaceIdol_A.glb",
    "SM_GiantAnchor_A.glb",
    "SM_HalfBuriedShipwreck_A.glb",
    "SM_BrokenMast_A.glb",
    "SM_StoneRing_A.glb",
    "SM_CampfireRuins_A.glb",
)

for filename in FILES:
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SOURCE, filename)
    task.destination_path = DEST
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    unreal.log(f"IMPORTED_PIRATE_LANDMARK {filename}: {task.imported_object_paths}")

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log("TREASURE_PIRATE_LANDMARK_IMPORT_COMPLETE")
