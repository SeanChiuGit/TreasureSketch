import bpy
import math
import os
from mathutils import Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "SourceAssets", "IslandLandmarks"))
os.makedirs(ROOT, exist_ok=True)

bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)


def material(name, color, roughness=0.82):
    value = bpy.data.materials.new(name)
    value.diffuse_color = (*color, 1.0)
    value.use_nodes = True
    bsdf = value.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    return value


WOOD = material("M_LandmarkWood", (0.24, 0.085, 0.025))
WOOD_LIGHT = material("M_LandmarkWoodLight", (0.46, 0.22, 0.07))
RED = material("M_LandmarkRed", (0.58, 0.025, 0.018))
CREAM = material("M_LandmarkCream", (0.78, 0.64, 0.39))
MUSHROOM_RED = material("M_MushroomRed", (0.72, 0.025, 0.03))
MUSHROOM_SPOT = material("M_MushroomSpot", (0.90, 0.78, 0.55))
STONE = material("M_LandmarkStone", (0.34, 0.37, 0.34))
STONE_DARK = material("M_LandmarkStoneDark", (0.19, 0.22, 0.20))
MOSS = material("M_LandmarkMoss", (0.08, 0.28, 0.065))
STATUE = material("M_Statue", (0.25, 0.33, 0.31))
ROOF = material("M_CabinRoof", (0.20, 0.055, 0.025))


def activate_collection(name):
    collection = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(collection)
    bpy.context.view_layer.active_layer_collection = bpy.context.view_layer.layer_collection.children[collection.name]
    return collection


def cube(name, location, scale, mat, rotation=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1, location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    obj.data.materials.append(mat)
    return obj


def cylinder(name, radius, depth, location, mat, vertices=10, rotation=(0, 0, 0)):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(mat)
    return obj


def ico(name, radius, location, scale, mat, subdivisions=1):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=subdivisions, radius=radius, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    obj.data.materials.append(mat)
    return obj


def join_export(collection, name):
    bpy.ops.object.select_all(action="DESELECT")
    objects = [obj for obj in collection.objects if obj.type == "MESH"]
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
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    bpy.context.scene.cursor.location = (0, 0, 0)
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    bpy.ops.object.select_all(action="DESELECT")
    result.select_set(True)
    bpy.context.view_layer.objects.active = result
    bpy.ops.export_scene.gltf(
        filepath=os.path.join(ROOT, name + ".glb"),
        use_selection=True,
        export_format="GLB",
        export_apply=True,
        export_animations=False,
    )
    return result


# Giant mushroom: exaggerated silhouette visible from across the island.
collection = activate_collection("GiantMushroom")
cylinder("Stem", 0.72, 4.8, (0, 0, 2.4), CREAM, 10)
ico("Cap", 1.0, (0, 0, 5.05), (2.7, 2.7, 0.72), MUSHROOM_RED, 2)
for angle in (0.2, 1.35, 2.4, 3.6, 4.8, 5.7):
    ico("Spot", 0.24, (math.cos(angle) * 1.55, math.sin(angle) * 1.55, 5.55), (1.0, 1.0, 0.42), MUSHROOM_SPOT)
mushroom = join_export(collection, "SM_GiantMushroom_A")


# Red windmill: red blades make it easy to name and draw.
collection = activate_collection("RedWindmill")
cylinder("Tower", 1.12, 5.6, (0, 0, 2.8), CREAM, 10)
bpy.ops.mesh.primitive_cone_add(vertices=10, radius1=1.45, radius2=0, depth=1.6, location=(0, 0, 6.35))
bpy.context.object.data.materials.append(ROOF)
cylinder("Axle", 0.22, 0.8, (0, -1.35, 5.15), WOOD, 10, (math.radians(90), 0, 0))
for angle in (0, math.pi / 2, math.pi, math.pi * 1.5):
    x = math.cos(angle) * 1.7
    z = 5.15 + math.sin(angle) * 1.7
    cube("Blade", (x, -1.78, z), (1.55, 0.12, 0.28), RED, (0, angle, 0))
cube("Door", (0, -1.05, 1.15), (0.42, 0.10, 0.85), WOOD)
windmill = join_export(collection, "SM_RedWindmill_A")


# Two twisted dead trees form a gate-like landmark.
collection = activate_collection("TwistedTwinTrees")
for side in (-1, 1):
    x = side * 1.35
    for index in range(4):
        cylinder(
            "TwistedTrunk",
            0.34 - index * 0.045,
            1.75,
            (x + side * index * 0.16, 0, 0.82 + index * 1.45),
            WOOD,
            8,
            (0, side * math.radians(8 + index * 3), side * math.radians(5)),
        )
    cylinder("Branch", 0.18, 2.1, (x + side * 0.82, 0, 4.8), WOOD, 7, (0, side * math.radians(62), 0))
    cylinder("Branch", 0.14, 1.65, (x + side * 0.28, 0.35, 5.45), WOOD, 7, (math.radians(45), side * math.radians(25), 0))
twisted_trees = join_export(collection, "SM_TwistedTwinTrees_A")


# Block-built stone gate with a strong, readable arch silhouette.
collection = activate_collection("StoneArch")
cube("LeftPillar", (-1.45, 0, 2.0), (0.66, 0.82, 2.0), STONE_DARK)
cube("RightPillar", (1.45, 0, 2.0), (0.66, 0.82, 2.0), STONE)
cube("Capstone", (0, 0, 4.35), (2.15, 0.88, 0.56), STONE)
cube("Moss", (-1.48, -0.84, 2.25), (0.44, 0.05, 0.25), MOSS, (0, 0, math.radians(12)))
cube("Moss", (0.20, -0.90, 4.38), (0.70, 0.05, 0.18), MOSS, (0, 0, math.radians(-7)))
stone_arch = join_export(collection, "SM_StoneArch_A")


# Strange three-eyed statue with a strong, asymmetric outline.
collection = activate_collection("StrangeStatue")
cylinder("Pedestal", 1.15, 0.75, (0, 0, 0.38), STONE_DARK, 7)
cube("Body", (0, 0, 2.0), (0.72, 0.55, 1.45), STATUE, (0, 0, math.radians(8)))
ico("Head", 0.78, (0.05, 0, 3.75), (0.82, 0.68, 1.0), STATUE)
for eye_x in (-0.30, 0, 0.30):
    ico("Eye", 0.105, (eye_x, -0.64, 3.88 + abs(eye_x) * 0.25), (1, 0.45, 1), MUSHROOM_SPOT)
cylinder("Horn", 0.16, 1.45, (-0.48, 0, 4.75), STATUE, 7, (0, math.radians(-24), 0))
cylinder("Horn", 0.16, 1.45, (0.58, 0, 4.72), STATUE, 7, (0, math.radians(24), 0))
statue = join_export(collection, "SM_StrangeStatue_A")


# Small cabin with a red roof, door and chimney.
collection = activate_collection("SmallCabin")
cube("Cabin", (0, 0, 1.45), (1.9, 1.5, 1.45), WOOD_LIGHT)
bpy.ops.mesh.primitive_cone_add(vertices=4, radius1=2.7, radius2=0, depth=1.75, location=(0, 0, 3.65), rotation=(0, 0, math.radians(45)))
bpy.context.object.name = "CabinRoof"
bpy.context.object.data.materials.append(ROOF)
cube("Door", (0, -1.52, 1.15), (0.48, 0.08, 0.92), WOOD)
cube("Window", (-1.05, -1.53, 1.65), (0.40, 0.07, 0.40), CREAM)
cylinder("Chimney", 0.28, 1.85, (1.0, 0.55, 4.1), STONE_DARK, 8)
cabin = join_export(collection, "SM_SmallCabin_A")


# Arrange all assets for one review render.
preview_positions = {
    mushroom: (-6.2, 3.4, 0),
    windmill: (0, 4.0, 0),
    twisted_trees: (6.0, 3.5, 0),
    stone_arch: (-5.6, -3.8, 0),
    statue: (0.2, -3.8, 0),
    cabin: (6.0, -3.8, 0),
}
for obj, position in preview_positions.items():
    obj.location = position

bpy.ops.mesh.primitive_plane_add(size=28, location=(0, 0, -0.04))
ground = bpy.context.object
ground.data.materials.append(material("M_LandmarkPreviewGround", (0.30, 0.43, 0.16)))
bpy.ops.object.light_add(type="SUN", location=(0, 0, 14))
bpy.context.object.rotation_euler = (math.radians(32), math.radians(-22), math.radians(24))
bpy.context.object.data.energy = 3.2
bpy.ops.object.camera_add(location=(18, -25, 16))
camera = bpy.context.object
camera.rotation_euler = ((Vector((0, 0, 2.4)) - camera.location).to_track_quat("-Z", "Y").to_euler())
bpy.context.scene.camera = camera
bpy.context.scene.render.engine = "BLENDER_EEVEE"
bpy.context.scene.render.resolution_x = 1280
bpy.context.scene.render.resolution_y = 800
bpy.context.scene.render.resolution_percentage = 100
bpy.context.scene.render.image_settings.file_format = "PNG"
bpy.context.scene.render.filepath = os.path.join(ROOT, "IslandLandmarks_Preview.png")
bpy.context.scene.world.color = (0.08, 0.13, 0.20)
bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ROOT, "IslandLandmarks_Source.blend"))
print("TREASURE_LANDMARK_ASSETS_CREATED", ROOT)
