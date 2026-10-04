# Create the local prop picker outline without altering source materials.
import unreal
path='/Game/UI/Materials/M_PropSelectionOutline'
mat=unreal.load_asset(path)
if mat is None:
 mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_PropSelectionOutline','/Game/UI/Materials',unreal.Material,unreal.MaterialFactoryNew())
unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_POST_PROCESS)
mat.set_editor_property('blendable_location',unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
scene=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionSceneTexture,-600,0)
scene.set_editor_property('scene_texture_id',unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
stencil=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionSceneTexture,-600,200)
stencil.set_editor_property('scene_texture_id',unreal.SceneTextureId.PPI_CUSTOM_STENCIL)
custom=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionCustom,-200,0)
inputs=[]
for name in ['Scene','Stencil','Texel']:
 i=unreal.CustomInput();i.set_editor_property('input_name',name);inputs.append(i)
custom.set_editor_property('inputs',inputs)
custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
custom.set_editor_property('code',r'''
float2 uv = GetDefaultSceneTextureUV(Parameters, 25);
float selected = abs(Stencil.r - 253.0) < 0.5;
float edge = 0.0;
float2 offsets[8] = {float2(3,0),float2(-3,0),float2(0,3),float2(0,-3),
 float2(2,2),float2(2,-2),float2(-2,2),float2(-2,-2)};
[unroll] for (int i=0; i<8; ++i) {
 float value=CalcSceneCustomStencil(uint2((uv+offsets[i]*Texel)*View.BufferSizeAndInvSize.xy));
 edge=max(edge,(abs(value-253.0)<0.5)?1.0:0.0);
}
float border=edge*(1.0-selected);
return lerp(Scene.rgb,float3(1.0,0.85,0.03),border);
''')
assert unreal.MaterialEditingLibrary.connect_material_expressions(scene,'Color',custom,'Scene')
assert unreal.MaterialEditingLibrary.connect_material_expressions(stencil,'Color',custom,'Stencil')
assert unreal.MaterialEditingLibrary.connect_material_expressions(stencil,'InvSize',custom,'Texel')
assert unreal.MaterialEditingLibrary.connect_material_property(custom,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
unreal.MaterialEditingLibrary.recompile_material(mat)
unreal.MaterialEditingLibrary.get_statistics(mat)
assert unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False)
unreal.log('PROP_OUTLINE_MATERIAL_READY')
