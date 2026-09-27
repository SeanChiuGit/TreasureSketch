import bpy
import math
import os
from mathutils import Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "SourceAssets", "JungleRuins"))
os.makedirs(ROOT, exist_ok=True)
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)


def mat(name, color):
    value = bpy.data.materials.new(name)
    value.diffuse_color = (*color, 1)
    value.use_nodes = True
    value.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (*color, 1)
    value.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.88
    return value


STONE = mat("M_RuinStone", (0.30, 0.34, 0.27))
STONE_DARK = mat("M_RuinStoneDark", (0.13, 0.16, 0.13))
STONE_LIGHT = mat("M_RuinStoneLight", (0.48, 0.49, 0.38))
MOSS = mat("M_RuinMoss", (0.06, 0.25, 0.055))
WOOD = mat("M_RuinWood", (0.25, 0.105, 0.035))
ROOF = mat("M_RuinRoof", (0.17, 0.12, 0.07))
VOID = mat("M_RuinInteriorDark", (0.018, 0.023, 0.019))


def collection(name):
    c = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(c)
    bpy.context.view_layer.active_layer_collection = bpy.context.view_layer.layer_collection.children[c.name]
    return c


def cube(name, loc, scale, material, rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc, rotation=rot)
    o = bpy.context.object
    o.name = name
    o.scale = scale
    o.data.materials.append(material)
    return o


def cylinder(name, loc, radius, depth, material, vertices=8):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=loc)
    o = bpy.context.object
    o.name = name
    o.data.materials.append(material)
    return o


def stair_run(origin, count, direction=(1, 0), width=2.3, tread=0.48, rise=0.34, material=STONE):
    ox, oy, oz = origin
    dx, dy = direction
    for i in range(count):
        cube("WalkableStep", (ox + dx * tread * i, oy + dy * tread * i, oz + rise * (i + 1) * 0.5),
             (tread * 1.04 if dx else width, tread * 1.04 if dy else width, rise * (i + 1)), material)


def join_export(group, name):
    bpy.ops.object.select_all(action="DESELECT")
    objs = [o for o in group.objects if o.type == "MESH"]
    for o in objs:
        o.select_set(True)
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        o.select_set(False)
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
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


# Explorer hut rebuilt on a single dimension grid so floors, walls, doorway and roof meet cleanly.
g = collection("ExplorerHut")
cube("Floor", (0, 0, 0.18), (6.4, 5.2, 0.36), STONE_DARK)
cube("BackWall", (0, 2.45, 1.95), (6.4, 0.30, 3.2), STONE)
cube("LeftWall", (-3.05, 0, 1.95), (0.30, 4.6, 3.2), STONE)
cube("RightWall", (3.05, 0, 1.95), (0.30, 4.6, 3.2), STONE)
cube("FrontLeft", (-2.2, -2.45, 1.95), (2.0, 0.30, 3.2), STONE)
cube("FrontRight", (2.2, -2.45, 1.95), (2.0, 0.30, 3.2), STONE)
cube("DoorLintel", (0, -2.45, 3.25), (2.4, 0.30, 0.60), STONE_LIGHT)
cube("Porch", (0, -3.05, 0.12), (3.8, 1.0, 0.24), STONE_DARK)
cube("Table", (0.8, 0.55, 0.82), (1.5, 0.9, 0.16), WOOD)
for x in (-2.75, 2.75):
    cube("RoofBeam", (x, 0, 3.72), (0.18, 5.5, 0.18), WOOD)
cube("RoofLeft", (-1.55, 0, 4.02), (3.55, 5.8, 0.22), ROOF, (0, math.radians(-14), 0))
cube("RoofRight", (1.55, 0, 4.02), (3.55, 5.8, 0.22), ROOF, (0, math.radians(14), 0))
for z in (0.65, 1.3, 1.95):
    cube("Moss", (-3.22, 0.9, z), (0.06, 0.70, 0.18), MOSS)
hut = join_export(g, "SM_RuinExplorerHut_A")


# Coherent two-level temple: one readable stepped mass with a complete upper shrine.
g = collection("TwoLevelTemple")
cube("LowerPlatform", (0, 0.35, 0.35), (9.6, 8.0, 0.70), STONE_DARK)
cube("MiddlePlatform", (0, 0.75, 1.15), (8.2, 6.8, 0.90), STONE)
cube("UpperPlatform", (0, 1.20, 2.0), (6.8, 5.5, 0.80), STONE_LIGHT)
stair_run((0, -4.10, 0), 8, direction=(0, 1), width=2.8, tread=0.48, rise=0.25)
cube("ShrineBack", (0, 3.55, 3.85), (6.2, 0.35, 3.7), STONE)
cube("ShrineLeft", (-2.92, 2.15, 3.85), (0.35, 2.8, 3.7), STONE)
cube("ShrineRight", (2.92, 2.15, 3.85), (0.35, 2.8, 3.7), STONE)
for x in (-2.25, 2.25):
    cylinder("TempleColumn", (x, 0.52, 3.85), 0.34, 3.7, STONE_LIGHT, 8)
cube("ShrineRoof", (0, 2.05, 5.85), (6.8, 3.8, 0.42), STONE_DARK)
cube("Altar", (0, 2.75, 2.75), (2.0, 1.15, 0.75), STONE_LIGHT)
cube("MossBand", (-1.55, 3.34, 4.75), (1.2, 0.06, 0.24), MOSS)
temple = join_export(g, "SM_RuinTwoLevelTemple_A")


# Square watchtower with continuous walls, a real doorway, upper floor and restrained damage.
g = collection("Watchtower")
cube("GroundFloor", (0, 0, 0.22), (5.6, 5.6, 0.44), STONE_DARK)
cube("BackWall", (0, 2.60, 3.0), (5.6, 0.38, 5.6), STONE)
cube("LeftWall", (-2.60, 0, 3.0), (0.38, 5.2, 5.6), STONE)
cube("RightWall", (2.60, 0, 3.0), (0.38, 5.2, 5.6), STONE)
cube("FrontLeft", (-1.85, -2.60, 3.0), (1.8, 0.38, 5.6), STONE)
cube("FrontRight", (1.85, -2.60, 3.0), (1.8, 0.38, 5.6), STONE)
cube("DoorLintel", (0, -2.60, 5.0), (1.9, 0.38, 1.6), STONE_LIGHT)
cube("UpperFloor", (0, 0, 3.25), (5.0, 5.0, 0.32), STONE_DARK)
stair_run((-4.65, -1.75, 0), 9, direction=(1, 0), width=1.35, tread=0.48, rise=0.36)
cube("StairLanding", (-0.45, -1.75, 3.28), (1.3, 1.55, 0.30), STONE)
for x, y in ((-2.35, -2.35), (0, -2.35), (2.35, -2.35), (-2.35, 2.35), (0, 2.35)):
    cube("Crenellation", (x, y, 6.25), (0.72, 0.72, 0.9), STONE_LIGHT)
tower = join_export(g, "SM_RuinWatchtower_A")


# Roofed shrine hall with a continuous room and a clear front colonnade.
g = collection("ShrineHall")
cube("HallFloor", (0, 0, 0.22), (8.4, 6.4, 0.44), STONE_DARK)
cube("HallBack", (0, 3.0, 2.25), (8.4, 0.40, 4.1), STONE)
cube("HallLeft", (-4.0, 0.3, 2.25), (0.40, 5.8, 4.1), STONE)
cube("HallRight", (4.0, 0.3, 2.25), (0.40, 5.8, 4.1), STONE)
for x in (-3.0, -1.0, 1.0, 3.0):
    cylinder("FrontColumn", (x, -2.72, 2.3), 0.34, 4.2, STONE_LIGHT, 8)
cube("Roof", (0, 0.25, 4.48), (8.8, 6.3, 0.42), STONE_DARK)
cube("RaisedAltar", (0, 2.0, 0.82), (2.3, 1.4, 0.95), STONE_LIGHT)
cube("DarkNiche", (0, 2.78, 2.35), (1.9, 0.06, 2.0), VOID)
cube("BrokenRoofGap", (3.35, -1.85, 4.72), (1.35, 1.15, 0.18), MOSS)
hall = join_export(g, "SM_RuinShrineHall_A")


# Solid crypt entrance with retaining walls, deep doorway and one continuous roof mass.
g = collection("CryptEntrance")
cube("MoundBase", (0, 0.9, 0.35), (7.2, 6.4, 0.70), STONE_DARK)
cube("CryptBody", (0, 1.25, 2.0), (6.4, 5.0, 3.3), STONE)
cube("FacadeLeft", (-2.05, -1.45, 2.0), (2.0, 0.55, 3.3), STONE_LIGHT)
cube("FacadeRight", (2.05, -1.45, 2.0), (2.0, 0.55, 3.3), STONE_LIGHT)
cube("FacadeTop", (0, -1.45, 3.65), (6.1, 0.55, 0.75), STONE_LIGHT)
cube("DarkDoor", (0, -1.76, 1.75), (1.8, 0.08, 2.8), VOID)
cube("StoneRoof", (0, 1.0, 3.95), (7.0, 5.8, 0.55), STONE_DARK)
for i in range(5):
    cube("DownStep", (0, -3.0 + i * 0.42, 0.10 - i * 0.08), (2.2, 0.46, 0.22), STONE)
for x in (-3.1, 3.1):
    cylinder("MarkerPillar", (x, -1.5, 1.8), 0.34, 3.6, STONE, 8)
cube("CryptMoss", (1.65, -1.77, 3.25), (1.1, 0.05, 0.22), MOSS)
crypt = join_export(g, "SM_RuinCryptEntrance_A")


placements = {
    hut: (-11.5, 6.5, 0), temple: (0, 6.2, 0), tower: (11.0, 6.0, 0),
    hall: (-6.5, -6.0, 0), crypt: (7.0, -6.0, 0),
}
for obj, pos in placements.items():
    obj.location = pos

bpy.ops.mesh.primitive_plane_add(size=42, location=(0, 0, -0.05))
bpy.context.object.data.materials.append(mat("M_RuinPreviewGround", (0.16, 0.25, 0.09)))
bpy.ops.object.light_add(type="SUN", location=(0, 0, 18))
bpy.context.object.rotation_euler = (math.radians(28), math.radians(-18), math.radians(24))
bpy.context.object.data.energy = 3.4
bpy.ops.object.camera_add(location=(27, -36, 25))
camera = bpy.context.object
camera.rotation_euler = ((Vector((0, 0, 2.1)) - camera.location).to_track_quat("-Z", "Y").to_euler())
bpy.context.scene.camera = camera
bpy.context.scene.render.engine = "BLENDER_EEVEE"
bpy.context.scene.render.resolution_x = 1440
bpy.context.scene.render.resolution_y = 900
bpy.context.scene.render.resolution_percentage = 100
bpy.context.scene.render.image_settings.file_format = "PNG"
bpy.context.scene.render.filepath = os.path.join(ROOT, "JungleRuins_Preview.png")
bpy.context.scene.world.color = (0.055, 0.09, 0.045)
bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ROOT, "JungleRuins_Source.blend"))
print("TREASURE_JUNGLE_RUINS_CREATED", ROOT)
