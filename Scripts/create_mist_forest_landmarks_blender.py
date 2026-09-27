import bpy
import math
import os
from mathutils import Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "SourceAssets", "MistForestLandmarks"))
os.makedirs(ROOT, exist_ok=True)
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)


def mat(name, color, roughness=0.9, metallic=0.0):
    value = bpy.data.materials.new(name)
    value.diffuse_color = (*color, 1)
    value.use_nodes = True
    bsdf = value.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*color, 1)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    return value


ROCK = mat("M_ForestRock", (0.22, 0.25, 0.21))
ROCK_LIGHT = mat("M_ForestRockLight", (0.34, 0.37, 0.31))
ROCK_DARK = mat("M_ForestRockDark", (0.105, 0.12, 0.105))
MOSS = mat("M_ForestMoss", (0.08, 0.23, 0.045))
BARK = mat("M_StumpBark", (0.10, 0.052, 0.027))
BARK_LIGHT = mat("M_StumpCut", (0.30, 0.17, 0.075))
WATER = mat("M_ForestPondWater", (0.025, 0.20, 0.22), 0.25, 0.05)
REED = mat("M_ForestReed", (0.16, 0.30, 0.045))
LILY = mat("M_ForestLily", (0.06, 0.31, 0.075))
MUSHROOM = mat("M_ForestMushroomCap", (0.62, 0.10, 0.055))
MUSHROOM_STEM = mat("M_ForestMushroomStem", (0.72, 0.63, 0.42))
GROUND = mat("M_ForestPreviewGround", (0.055, 0.12, 0.038))


def collection(name):
    group = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(group)
    bpy.context.view_layer.active_layer_collection = bpy.context.view_layer.layer_collection.children[group.name]
    return group


def ico(name, location, scale, material, subdivisions=1):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=subdivisions, radius=1, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    obj.data.materials.append(material)
    return obj


def cube(name, location, scale, material, rotation=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1, location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    obj.data.materials.append(material)
    return obj


def cylinder(name, location, radius, depth, material, vertices=10, rotation=(0, 0, 0)):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth,
                                       location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(material)
    return obj


def torus(name, location, major_radius, minor_radius, material, rotation=(0, 0, 0)):
    bpy.ops.mesh.primitive_torus_add(major_radius=major_radius, minor_radius=minor_radius,
                                    major_segments=12, minor_segments=6, location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(material)
    return obj


def export(group, name):
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


# 1. Moss boulder: asymmetrical mound with one unmistakable green cap.
g = collection("MossBoulder")
ico("Boulder", (0, 0, 1.05), (2.0, 1.55, 1.15), ROCK, 2)
ico("MossCap", (-0.35, 0.05, 2.02), (1.25, 1.02, 0.18), MOSS, 1)
moss_boulder = export(g, "SM_MistForestMossBoulder_A")

# 2. Split boulder: a readable V-shaped crack instead of a painted line.
g = collection("SplitBoulder")
ico("LeftHalf", (-0.72, 0, 1.15), (1.25, 1.45, 1.35), ROCK_LIGHT, 1)
ico("RightHalf", (0.78, 0.06, 1.05), (1.15, 1.38, 1.23), ROCK, 1)
cube("DarkCrevice", (0.02, -0.02, 1.25), (0.18, 2.05, 1.65), ROCK_DARK, (0, 0, math.radians(8)))
split_boulder = export(g, "SM_MistForestSplitBoulder_A")

# 3. Three-stone stack: deliberately unbalanced silhouette.
g = collection("StoneStack")
ico("BaseStone", (0, 0, 0.62), (1.75, 1.35, 0.72), ROCK_DARK, 1)
ico("MiddleStone", (0.28, 0, 1.50), (1.25, 1.02, 0.66), ROCK, 1)
ico("TopStone", (-0.25, 0.02, 2.25), (0.78, 0.70, 0.58), ROCK_LIGHT, 1)
stone_stack = export(g, "SM_MistForestThreeStoneStack_A")

# 4. Flat overlook slab: low enough to jump onto and broad enough for treasure placement.
g = collection("FlatStoneSlab")
ico("FlatSlab", (0, 0, 0.55), (2.45, 1.85, 0.62), ROCK_LIGHT, 1)
cube("MossStripe", (0.65, 0.1, 1.12), (1.15, 1.2, 0.10), MOSS, (0, 0, math.radians(12)))
flat_slab = export(g, "SM_MistForestFlatStoneSlab_A")

# 5. Small oval pond with reeds, lily pads and a broken stone edge.
g = collection("ForestPond")
cylinder("PondWater", (0, 0, 0.04), 3.0, 0.08, WATER, 32)
bpy.context.object.scale.y = 0.62
for i in range(14):
    angle = i * math.tau / 14
    if 1.0 < angle < 2.0:
        continue
    ico("BankStone", (math.cos(angle) * 3.0, math.sin(angle) * 1.88, 0.28),
        (0.52, 0.38, 0.32), ROCK if i % 2 else ROCK_DARK, 1)
for x, y, size in ((-0.8, 0.15, 0.36), (0.65, -0.42, 0.31), (1.25, 0.25, 0.25)):
    cylinder("LilyPad", (x, y, 0.10), size, 0.04, LILY, 12)
for x in (-2.2, -1.85, 2.0, 2.28):
    for offset in (-0.18, 0.0, 0.18):
        cylinder("Reed", (x + offset, 0.72 if x < 0 else -0.62, 0.78), 0.045, 1.55, REED, 6)
pond = export(g, "SM_MistForestSmallPond_A")

# 6. Reed cluster: dense eye-level screen but no collision-worthy mass.
g = collection("ReedCluster")
for i in range(22):
    angle = i * 2.399
    radius = 0.18 * math.sqrt(i)
    height = 1.25 + (i % 5) * 0.19
    cylinder("ReedStem", (math.cos(angle) * radius, math.sin(angle) * radius, height * 0.5),
             0.045, height, REED, 6)
reeds = export(g, "SM_MistForestReedCluster_A")

# 7. Hollow stump: thick ring and a black opening that reads from normal eye height.
g = collection("HollowStump")
torus("StumpRim", (0, 0, 1.55), 1.05, 0.32, BARK_LIGHT)
for i in range(10):
    angle = i * math.tau / 10
    cylinder("StumpWall", (math.cos(angle) * 0.88, math.sin(angle) * 0.88, 0.78),
             0.34, 1.55 + (i % 3) * 0.20, BARK, 7)
cylinder("DarkHollow", (0, 0, 1.55), 0.72, 0.05, ROCK_DARK, 16)
hollow_stump = export(g, "SM_MistForestHollowStump_A")

# 8. Mushroom ring: obvious from above, confusing from ground level in a useful way.
g = collection("MushroomRing")
for i in range(12):
    angle = i * math.tau / 12
    radius = 1.55 + (i % 3) * 0.08
    height = 0.42 + (i % 4) * 0.08
    x, y = math.cos(angle) * radius, math.sin(angle) * radius
    cylinder("MushroomStem", (x, y, height * 0.5), 0.10, height, MUSHROOM_STEM, 7)
    ico("MushroomCap", (x, y, height), (0.28, 0.28, 0.15), MUSHROOM, 1)
mushroom_ring = export(g, "SM_MistForestMushroomRing_A")


placements = {
    moss_boulder: (-11.5, 5.0, 0), split_boulder: (-6.0, 5.0, 0),
    stone_stack: (-0.5, 5.0, 0), flat_slab: (5.0, 5.0, 0),
    pond: (11.3, 5.0, 0), reeds: (-8.2, -4.0, 0),
    hollow_stump: (0, -4.0, 0), mushroom_ring: (7.0, -4.0, 0),
}
for obj, position in placements.items():
    obj.location = position

bpy.ops.mesh.primitive_plane_add(size=39, location=(0, 1.5, -0.08))
bpy.context.object.data.materials.append(GROUND)
bpy.ops.object.light_add(type="SUN", location=(0, 0, 18))
bpy.context.object.rotation_euler = (math.radians(32), math.radians(-18), math.radians(24))
bpy.context.object.data.energy = 3.0
bpy.ops.object.camera_add(location=(27, -34, 21))
camera = bpy.context.object
camera.rotation_euler = ((Vector((0, 2.0, 1.2)) - camera.location).to_track_quat("-Z", "Y").to_euler())
bpy.context.scene.camera = camera
bpy.context.scene.render.engine = "BLENDER_EEVEE"
bpy.context.scene.render.resolution_x = 1500
bpy.context.scene.render.resolution_y = 950
bpy.context.scene.render.resolution_percentage = 100
bpy.context.scene.render.image_settings.file_format = "PNG"
bpy.context.scene.render.filepath = os.path.join(ROOT, "MistForestLandmarks_Preview.png")
bpy.context.scene.world.color = (0.03, 0.055, 0.035)
bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ROOT, "MistForestLandmarks_Source.blend"))
print("TREASURE_MIST_FOREST_LANDMARKS_CREATED", ROOT)
