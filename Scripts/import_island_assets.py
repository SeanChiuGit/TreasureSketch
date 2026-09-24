import os
import unreal

SOURCE = r"C:\UEProjects\TreasureSketch\SourceAssets\Island"
DEST = "/Game/IslandAssets/Prototype"
LEVEL = "/Game/Maps/L_IslandAssetsPreview"

for filename in ("SM_PalmTree_A.glb", "SM_RockCluster_A.glb", "SM_Bush_A.glb", "SM_Driftwood_A.glb"):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SOURCE, filename)
    task.destination_path = DEST
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    unreal.log(f"IMPORTED {filename}: {task.imported_object_paths}")

unreal.EditorLevelLibrary.new_level(LEVEL)

floor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -55))
floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube.Cube'))
floor.set_actor_scale3d(unreal.Vector(22, 14, 1))
floor.set_actor_label('Preview_Sand_Floor')

placements = [
    ('SM_PalmTree_A', unreal.Vector(200, -650, 0)),
    ('SM_RockCluster_A', unreal.Vector(100, -150, 0)),
    ('SM_Bush_A', unreal.Vector(100, 350, 0)),
    ('SM_Driftwood_A', unreal.Vector(150, 750, 0)),
]

assets = unreal.EditorAssetLibrary.list_assets(DEST, recursive=True, include_folder=False)
for name, location in placements:
    match = next((p for p in assets if p.endswith('/' + name + '.' + name)), None)
    if not match:
        unreal.log_warning(f"STATIC MESH NOT FOUND {name}; assets={assets}")
        continue
    mesh = unreal.load_asset(match)
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, location)
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.set_actor_label(name)

player = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-1300, 0, 260), unreal.Rotator(0, 0, 0))
player.set_actor_label('Preview_PlayerStart')

sun = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0,0,1600), unreal.Rotator(-42,-35,0))
sun.light_component.set_editor_property('intensity', 8.0)
sky = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0,0,1000))
sky.light_component.set_editor_property('intensity', 1.2)
unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector())

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log(f"TREASURE_ISLAND_ASSET_PREVIEW_CREATED {LEVEL}")
