import bpy
import math
import os
from mathutils import Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "SourceAssets", "PirateLandmarks"))
os.makedirs(ROOT, exist_ok=True)

bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)


def mat(name, color, roughness=0.84):
    value = bpy.data.materials.new(name)
    value.diffuse_color = (*color, 1.0)
    value.use_nodes = True
    bsdf = value.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    return value


STONE = mat("M_PirateStone", (0.31, 0.34, 0.31))
STONE_DARK = mat("M_PirateStoneDark", (0.12, 0.14, 0.13))
STONE_LIGHT = mat("M_PirateStoneLight", (0.47, 0.49, 0.43))
MOSS = mat("M_PirateMoss", (0.06, 0.24, 0.05))
WOOD = mat("M_PirateWood", (0.22, 0.075, 0.02))
WOOD_LIGHT = mat("M_PirateWoodLight", (0.42, 0.19, 0.055))
METAL = mat("M_PirateMetal", (0.09, 0.11, 0.12), 0.60)
SAIL = mat("M_TornSail", (0.16, 0.025, 0.018))
EMBER = mat("M_Ember", (0.95, 0.19, 0.015), 0.45)
FLAME = mat("M_Flame", (1.0, 0.52, 0.025), 0.35)


def collection(name):
    value = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(value)
    bpy.context.view_layer.active_layer_collection = bpy.context.view_layer.layer_collection.children[value.name]
    return value


def cube(name, location, scale, material, rotation=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1, location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    obj.data.materials.append(material)
    return obj


def cylinder(name, radius, depth, location, material, vertices=9, rotation=(0, 0, 0)):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(material)
    return obj


def ico(name, radius, location, scale, material, subdivisions=1):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=subdivisions, radius=radius, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    obj.data.materials.append(material)
    return obj


def cone(name, radius, depth, location, material, vertices=8, rotation=(0, 0, 0)):
    bpy.ops.mesh.primitive_cone_add(vertices=vertices, radius1=radius, radius2=0, depth=depth, location=location, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(material)
    return obj


def triangle(name, vertices, material):
    mesh = bpy.data.meshes.new(name + "Mesh")
    mesh.from_pydata(vertices, [], [(0, 1, 2)])
    mesh.materials.append(material)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def join_export(source_collection, name):
    bpy.ops.object.select_all(action="DESELECT")
    objects = [obj for obj in source_collection.objects if obj.type == "MESH"]
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
    bpy.ops.export_scene.gltf(
        filepath=os.path.join(ROOT, name + ".glb"),
        use_selection=True,
        export_format="GLB",
        export_apply=True,
        export_animations=False,
    )
    return result


# Skull idol: round eyes and square jaw remain recognizable from far away.
group = collection("SkullIdol")
cylinder("Pedestal", 1.25, 0.72, (0, 0, 0.36), STONE_DARK, 7)
ico("Skull", 1.0, (0, 0, 2.35), (1.35, 0.82, 1.45), STONE_LIGHT, 2)
cube("Jaw", (0, -0.12, 1.2), (0.72, 0.55, 0.55), STONE_LIGHT)
ico("LeftSocket", 0.34, (-0.47, -0.76, 2.62), (1.0, 0.32, 1.1), STONE_DARK)
ico("RightSocket", 0.34, (0.47, -0.76, 2.62), (1.0, 0.32, 1.1), STONE_DARK)
cone("Nose", 0.22, 0.55, (0, -0.82, 2.05), STONE_DARK, 5, (math.radians(90), 0, 0))
for tooth_x in (-0.42, -0.14, 0.14, 0.42):
    cube("ToothGap", (tooth_x, -0.68, 1.25), (0.055, 0.05, 0.34), STONE_DARK)
skull = join_export(group, "SM_SkullIdol_A")


# Face idol: intentionally similar material and scale, but a long nose and slit eyes.
group = collection("FaceIdol")
cylinder("Pedestal", 1.18, 0.65, (0, 0, 0.33), STONE_DARK, 7)
cube("Head", (0, 0, 2.25), (1.02, 0.72, 1.72), STONE)
cube("Brow", (0, -0.76, 2.75), (0.86, 0.12, 0.16), STONE_LIGHT)
cube("LeftEye", (-0.43, -0.90, 2.55), (0.23, 0.06, 0.09), STONE_DARK)
cube("RightEye", (0.43, -0.90, 2.55), (0.23, 0.06, 0.09), STONE_DARK)
cube("Nose", (0, -1.02, 2.08), (0.20, 0.34, 0.62), STONE_LIGHT, (math.radians(-8), 0, 0))
cube("Mouth", (0, -0.78, 1.35), (0.50, 0.06, 0.10), STONE_DARK)
cube("Moss", (0.68, -0.79, 3.25), (0.25, 0.05, 0.30), MOSS, (0, 0, math.radians(-18)))
face = join_export(group, "SM_FaceIdol_A")


# Giant anchor: a simple icon with a circular eye and broad flukes.
group = collection("GiantAnchor")
cylinder("Shaft", 0.24, 5.2, (0, 0, 2.75), METAL, 10)
cylinder("Crossbar", 0.18, 3.3, (0, 0, 3.8), METAL, 10, (0, math.radians(90), 0))
bpy.ops.mesh.primitive_torus_add(major_radius=0.62, minor_radius=0.17, major_segments=12, minor_segments=6, location=(0, 0, 5.65))
bpy.context.object.data.materials.append(METAL)
cylinder("LeftArm", 0.22, 2.25, (-0.66, 0, 0.78), METAL, 9, (0, math.radians(-47), 0))
cylinder("RightArm", 0.22, 2.25, (0.66, 0, 0.78), METAL, 9, (0, math.radians(47), 0))
cone("LeftFluke", 0.62, 1.15, (-1.42, 0, 1.22), METAL, 5, (0, math.radians(-55), 0))
cone("RightFluke", 0.62, 1.15, (1.42, 0, 1.22), METAL, 5, (0, math.radians(55), 0))
anchor = join_export(group, "SM_GiantAnchor_A")


# Half-buried wreck: keel, ribs and irregular broken planks.
group = collection("Shipwreck")
cylinder("Keel", 0.28, 6.8, (0, 0, 0.55), WOOD, 9, (0, math.radians(90), 0))
for index, x in enumerate((-2.4, -1.2, 0, 1.2, 2.4)):
    cylinder("Rib", 0.20, 3.3 - abs(x) * 0.22, (x, 0, 1.35), WOOD_LIGHT, 8, (math.radians(90), 0, 0))
    cube("HullPlank", (x, -0.82, 1.1 + (index % 2) * 0.32), (0.72, 0.16, 0.22), WOOD, (0, math.radians((index - 2) * 7), math.radians(8)))
cylinder("BrokenMast", 0.23, 4.2, (0.65, 0.3, 2.25), WOOD, 9, (0, math.radians(-18), 0))
shipwreck = join_export(group, "SM_HalfBuriedShipwreck_A")


# Broken mast: can be mistaken for a dead tree when drawn quickly.
group = collection("BrokenMast")
cylinder("Mast", 0.28, 6.1, (0, 0, 2.9), WOOD, 9, (0, math.radians(12), 0))
cylinder("Yard", 0.17, 3.8, (0.55, 0, 4.55), WOOD_LIGHT, 8, (0, math.radians(90), math.radians(8)))
triangle("TornSail", [(0.45, -0.06, 4.45), (2.0, -0.06, 4.25), (0.65, -0.06, 2.15)], SAIL)
cube("Rope", (-0.38, 0, 1.0), (0.07, 0.07, 0.85), WOOD_LIGHT, (0, math.radians(-22), 0))
mast = join_export(group, "SM_BrokenMast_A")


def add_stone_ring(name, with_fire):
    group = collection(name)
    for index in range(9):
        angle = index * math.tau / 9
        ico("RingStone", 0.52, (math.cos(angle) * 1.35, math.sin(angle) * 1.35, 0.35), (1.0, 0.72, 0.62), STONE if index % 2 else STONE_DARK)
    if with_fire:
        cylinder("Log", 0.17, 2.15, (0, 0, 0.45), WOOD, 7, (0, math.radians(90), math.radians(38)))
        cylinder("Log", 0.17, 2.15, (0, 0, 0.45), WOOD_LIGHT, 7, (0, math.radians(90), math.radians(-38)))
        ico("Ember", 0.65, (0, 0, 0.65), (1, 1, 0.35), EMBER)
        cone("Flame", 0.55, 1.65, (0, 0, 1.45), FLAME, 7)
    return join_export(group, "SM_CampfireRuins_A" if with_fire else "SM_StoneRing_A")


stone_ring = add_stone_ring("StoneRing", False)
campfire = add_stone_ring("CampfireRuins", True)


placements = {
    skull: (-7.8, 3.7, 0),
    face: (-3.7, 3.7, 0),
    anchor: (1.0, 3.6, 0),
    shipwreck: (6.4, 3.4, 0),
    mast: (-6.0, -3.8, 0),
    stone_ring: (0, -3.8, 0),
    campfire: (5.4, -3.8, 0),
}
for obj, position in placements.items():
    obj.location = position

bpy.ops.mesh.primitive_plane_add(size=32, location=(0, 0, -0.04))
ground = bpy.context.object
ground.data.materials.append(mat("M_PiratePreviewSand", (0.53, 0.40, 0.20)))
bpy.ops.object.light_add(type="SUN", location=(0, 0, 14))
bpy.context.object.rotation_euler = (math.radians(32), math.radians(-20), math.radians(25))
bpy.context.object.data.energy = 3.2
bpy.ops.object.camera_add(location=(19, -27, 17))
camera = bpy.context.object
camera.rotation_euler = ((Vector((0, 0, 2.0)) - camera.location).to_track_quat("-Z", "Y").to_euler())
bpy.context.scene.camera = camera
bpy.context.scene.render.engine = "BLENDER_EEVEE"
bpy.context.scene.render.resolution_x = 1280
bpy.context.scene.render.resolution_y = 800
bpy.context.scene.render.resolution_percentage = 100
bpy.context.scene.render.image_settings.file_format = "PNG"
bpy.context.scene.render.filepath = os.path.join(ROOT, "PirateLandmarks_Preview.png")
bpy.context.scene.world.color = (0.07, 0.12, 0.18)
bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ROOT, "PirateLandmarks_Source.blend"))
print("TREASURE_PIRATE_LANDMARKS_CREATED", ROOT)

