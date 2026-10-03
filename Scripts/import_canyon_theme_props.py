import unreal
from pathlib import Path
root=Path(r'C:\Users\seanc\OneDrive\文档\ChatGPT\游戏研究\TreasureSketch')
names=['SM_SupplyCrate','SM_WaterBarrel','SM_MineCart','SM_Cactus','SM_OreCluster','SM_RockCluster','SM_ThreeStoneStack','SM_Campfire','SM_FallenLog','SM_SkullIdol']
for name in names:
    task=unreal.AssetImportTask()
    task.filename=str(root/'SourceAssets'/'CanyonTheme'/'FBX'/(name+'.fbx'))
    task.destination_path='/Game/IslandAssets/Canyon/Props/'+name
    task.destination_name=name
    task.automated=True;task.save=True;task.replace_existing=False
    opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_as_skeletal=False;opt.import_animations=False
    opt.static_mesh_import_data.combine_meshes=True
    opt.static_mesh_import_data.auto_generate_collision=True
    task.options=opt
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh=unreal.load_asset(task.destination_path+'/'+name)
    if not isinstance(mesh,unreal.StaticMesh):raise RuntimeError('Missing mesh '+name)
    unreal.log('CANYON_PROP_VERIFIED '+name)
unreal.EditorAssetLibrary.save_directory('/Game/IslandAssets/Canyon')
unreal.log('CANYON_THEME_IMPORT_COMPLETE 10')
