import bpy
import math
import os
from mathutils import Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "SourceAssets", "MistForest"))
os.makedirs(ROOT, exist_ok=True)
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)


def material(name, color, roughness=0.92):
    value = bpy.data.materials.new(name)
    value.diffuse_color = (*color, 1)
    value.use_nodes = True
    value.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (*color, 1)
    value.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = roughness
    return value


BARK = material("M_ForestBark", (0.075, 0.042, 0.024))
BARK_MOSS = material("M_ForestBarkMoss", (0.10, 0.17, 0.045))
LEAF_DARK = material("M_ForestLeafDark", (0.012, 0.075, 0.020))
LEAF = material("M_ForestLeaf", (0.025, 0.16, 0.038))
LEAF_LIGHT = material("M_ForestLeafLight", (0.075, 0.25, 0.065))
GROUND = material("M_ForestGround", (0.055, 0.105, 0.035))


def collection(name):
    group = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(group)
    bpy.context.view_layer.active_layer_collection = bpy.context.view_layer.layer_collection.children[group.name]
    return group


def cylinder(name, location, radius, depth, mat, vertices=10, rotation=(0, 0, 0)):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth,
                                       location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(mat)
    return obj


def crown(name, location, scale, mat):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=1, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    obj.data.materials.append(mat)
    return obj


def cube(name, location, scale, mat, rotation=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1, location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    obj.data.materials.append(mat)
    return obj


def join_export(group, name):
    bpy.ops.object.select_all(action="DESELECT")
    objects = [obj for obj in group.objects if obj.type == "MESH"]
    for obj in objects:
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        obj.select_set(False)
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    result = bpy.context.object
    result.name = name
    bpy.context.scene.cursor.location = (0, 0, 0)
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    bpy.ops.object.select_all(action="DESELECT")
    result.select_set(True)
    bpy.context.view_layer.objects.active = result
    bpy.ops.export_scene.gltf(filepath=os.path.join(ROOT, name + ".glb"), use_selection=True,
                              export_format="GLB", export_apply=True, export_animations=False)
    return result


# Massive ancient oak: one unmistakable umbrella silhouette with a hollow at its base.
group = collection("AncientOak")
cylinder("AncientTrunk", (0, 0, 4.2), 0.85, 8.4, BARK, 12)
for angle in range(0, 360, 45):
    radians = math.radians(angle)
    # High buttress at the trunk, followed by a long low root that visibly sinks into the soil.
    cube("RaisedButtress", (math.cos(radians) * 0.70, math.sin(radians) * 0.70, 1.05),
         (1.75, 0.42, 2.10), BARK_MOSS, (0, 0, radians))
    cube("SpreadingRoot", (math.cos(radians) * 2.05, math.sin(radians) * 2.05, 0.30),
         (2.25, 0.34, 0.55), BARK, (0, 0, radians))
for index, (x, y, z, sx, sy) in enumerate((
    (0, 0, 8.2, 3.4, 2.7), (-2.8, 0.2, 7.7, 2.8, 2.2), (2.7, 0.5, 7.8, 2.7, 2.3),
    (-1.1, 2.2, 8.0, 2.7, 2.1), (1.2, -2.1, 7.7, 2.8, 2.0), (0, 0.4, 9.6, 2.7, 2.2))):
    crown("DenseCrown", (x, y, z), (sx, sy, 1.65), LEAF_DARK if index % 2 else LEAF)
ancient = join_export(group, "SM_MistForestAncientOak_A")

# Twin-trunk tree: readable V silhouette even when its canopy blocks the background.
group = collection("TwinTrunk")
cylinder("LeftTrunk", (-0.65, 0, 4.0), 0.55, 8.0, BARK, 10, (0, math.radians(-9), 0))
cylinder("RightTrunk", (0.75, 0.1, 4.2), 0.52, 8.4, BARK_MOSS, 10, (0, math.radians(10), 0))
for index, point in enumerate(((-2.2, 0, 8.3), (0, 0, 9.1), (2.3, 0.2, 8.5), (-0.7, 2.0, 8.2), (1.0, -1.8, 8.0))):
    crown("TwinDenseCrown", point, (2.65, 2.25, 1.75), LEAF if index % 2 else LEAF_LIGHT)
twin = join_export(group, "SM_MistForestTwinTrunk_A")

# Low spreading beech: very dense eye-level obstruction and a broad horizontal silhouette.
group = collection("SpreadingBeech")
cylinder("BeechTrunk", (0, 0, 3.3), 0.62, 6.6, BARK, 11)
for angle in (-58, -28, 28, 58):
    radians = math.radians(angle)
    cylinder("LowBranch", (math.sin(radians) * 1.45, 0, 5.65), 0.28, 4.2,
             BARK_MOSS, 9, (0, radians, 0))
for index, point in enumerate(((-3.2, 0, 6.1), (-1.6, 0.5, 7.0), (0, 0, 7.3),
                               (1.7, -0.4, 7.0), (3.3, 0, 6.2), (0, 2.0, 6.6), (0, -2.0, 6.5))):
    crown("BeechDenseCrown", point, (2.35, 2.0, 1.6), LEAF_DARK if index % 3 == 0 else LEAF)
beech = join_export(group, "SM_MistForestSpreadingBeech_A")

# Weeping tree: hanging leaf masses form a curtain and make a strong navigation landmark.
group = collection("WeepingTree")
cylinder("WeepingTrunk", (0, 0, 3.7), 0.58, 7.4, BARK_MOSS, 11)
for index, (x, y) in enumerate(((-2.4, 0), (-1.2, 1.5), (0, 1.9), (1.3, 1.3), (2.4, 0),
                                (1.3, -1.4), (0, -1.9), (-1.3, -1.4))):
    crown("HangingCrown", (x, y, 6.8 - (index % 2) * 0.35), (1.55, 1.4, 2.7),
          LEAF_DARK if index % 2 else LEAF_LIGHT)
crown("TopCrown", (0, 0, 8.4), (2.7, 2.4, 1.55), LEAF)
weeping = join_export(group, "SM_MistForestWeepingTree_A")


for obj, position in {
    ancient: (-10.5, 4.0, 0), twin: (-3.4, 4.0, 0),
    beech: (4.2, 4.0, 0), weeping: (11.5, 4.0, 0),
}.items():
    obj.location = position

bpy.ops.mesh.primitive_plane_add(size=38, location=(0, 2.5, -0.08))
bpy.context.object.data.materials.append(GROUND)
bpy.ops.object.light_add(type="SUN", location=(0, 0, 18))
bpy.context.object.rotation_euler = (math.radians(32), math.radians(-18), math.radians(24))
bpy.context.object.data.energy = 2.6
bpy.ops.object.camera_add(location=(25, -32, 17))
camera = bpy.context.object
camera.rotation_euler = ((Vector((0, 3.5, 4.5)) - camera.location).to_track_quat("-Z", "Y").to_euler())
bpy.context.scene.camera = camera
bpy.context.scene.render.engine = "BLENDER_EEVEE"
bpy.context.scene.render.resolution_x = 1440
bpy.context.scene.render.resolution_y = 900
bpy.context.scene.render.resolution_percentage = 100
bpy.context.scene.render.image_settings.file_format = "PNG"
bpy.context.scene.world.color = (0.025, 0.045, 0.035)

# Two previews provide the same debug switch planned for UE: geometry is unchanged,
# only the separate atmospheric layer is enabled or disabled.
bpy.context.scene.render.filepath = os.path.join(ROOT, "MistForest_Clear_Preview.png")
bpy.ops.render.render(write_still=True)

# A separate volume object previews the weather layer without baking fog into
# any tree material. UE will use a separately toggleable fog component instead.
bpy.ops.mesh.primitive_cube_add(size=2, location=(0, 0, 11))
fog_box = bpy.context.object
fog_box.name = "PreviewFogVolume"
fog_box.scale = (32, 32, 18)
fog_material = bpy.data.materials.new("M_PreviewFogVolume")
fog_material.use_nodes = True
fog_nodes = fog_material.node_tree.nodes
fog_links = fog_material.node_tree.links
for node in list(fog_nodes):
    fog_nodes.remove(node)
fog_output = fog_nodes.new("ShaderNodeOutputMaterial")
fog_volume = fog_nodes.new("ShaderNodeVolumePrincipled")
fog_volume.inputs["Density"].default_value = 0.018
fog_volume.inputs["Color"].default_value = (0.58, 0.67, 0.61, 1)
fog_links.new(fog_volume.outputs["Volume"], fog_output.inputs["Volume"])
fog_box.data.materials.append(fog_material)
bpy.context.scene.render.filepath = os.path.join(ROOT, "MistForest_Fog_Preview.png")
bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ROOT, "MistForest_Source.blend"))
print("TREASURE_MIST_FOREST_CREATED", ROOT)
