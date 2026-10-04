"""Enable instanced rendering for the canyon modules used by the generator."""
import unreal

names = [f'SM_Canyon{part}_{variant}'
         for variant in ('Straight_A', 'Straight_B_Wide', 'Straight_C_Narrow')
         for part in ('Floor', 'Wall_L', 'Wall_R')]
names += [f'SM_CanyonTalus_{index:02d}' for index in range(1, 4)]
materials = {}
reparented = {}
localized = {}

def localize(interface):
    source = interface.get_path_name()
    if source in localized:
        return localized[source]
    assert isinstance(interface, (unreal.Material, unreal.MaterialInstance)), source
    local = interface
    if not source.startswith('/Game/IslandAssets/CanyonModules/'):
        path = '/Game/IslandAssets/CanyonModules/Materials/' + interface.get_name() + '_CanyonInstanced'
        local = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.EditorAssetLibrary.duplicate_asset(source, path)
        assert local is not None, path
    localized[source] = local
    if isinstance(local, unreal.MaterialInstance):
        parent = local.get_editor_property('parent')
        prepared = localize(parent)
        if parent != prepared:
            local.set_editor_property('parent', prepared)
            reparented[local.get_path_name()] = local
    else:
        materials[local.get_path_name()] = local
    return local

for name in names:
    mesh = unreal.load_asset(f'/Game/IslandAssets/CanyonModules/{name}/StaticMeshes/{name}')
    assert mesh is not None, name
    changed_mesh = False
    for index, slot in enumerate(mesh.get_editor_property('static_materials')):
        interface = slot.get_editor_property('material_interface')
        prepared = localize(interface)
        if interface != prepared:
            mesh.set_material(index, prepared)
            changed_mesh = True
    if changed_mesh:
        assert unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False), name
for path, material in materials.items():
    material.set_editor_property('used_with_instanced_static_meshes', True)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False), path
for path, instance in reparented.items():
    assert unreal.EditorAssetLibrary.save_loaded_asset(instance, only_if_is_dirty=False), path
unreal.log(f'CANYON_INSTANCED_MATERIALS_READY {len(materials)}')
