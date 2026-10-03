import bpy
from pathlib import Path
root=Path(__file__).resolve().parents[1]
out=root/'SourceAssets'/'CanyonTheme'/'FBX'
out.mkdir(parents=True,exist_ok=True)
assets={
'SM_SupplyCrate':'CanyonNewProps/01_SupplyCrate.glb',
'SM_WaterBarrel':'CanyonNewProps/02_WaterBarrel.glb',
'SM_MineCart':'CanyonNewProps/03_MineCart.glb',
'SM_Cactus':'CanyonNewProps/04_Cactus.glb',
'SM_OreCluster':'CanyonNewProps/05_OreCluster.glb',
'SM_RockCluster':'Island/SM_RockCluster_A.glb',
'SM_ThreeStoneStack':'MistForestLandmarks/SM_MistForestThreeStoneStack_A.glb',
'SM_Campfire':'PirateLandmarks/SM_CampfireRuins_A.glb',
'SM_FallenLog':'JungleNature/SM_FallenJungleLog_A.glb',
'SM_SkullIdol':'PirateLandmarks/SM_SkullIdol_A.glb'}
for name,path in assets.items():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(root/'SourceAssets'/path))
    bpy.ops.object.select_all(action='DESELECT')
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    for o in meshes:o.select_set(True)
    bpy.context.view_layer.objects.active=meshes[0]
    bpy.ops.object.join()
    obj=bpy.context.object
    obj.name=name
    world=obj.matrix_world.copy();obj.parent=None;obj.matrix_world=world
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    bpy.ops.export_scene.fbx(filepath=str(out/(name+'.fbx')),use_selection=True,object_types={'MESH'},bake_anim=False,path_mode='AUTO')
print('CANYON_EXPORT_COMPLETE')
