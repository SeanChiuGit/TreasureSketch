import bpy
import math
import os
import random
from mathutils import Vector

ROOT = r"C:\UEProjects\TreasureSketch\SourceAssets\Island"
os.makedirs(ROOT, exist_ok=True)

bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)

def mat(name, color):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1.0)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (*color, 1.0)
    bsdf.inputs['Roughness'].default_value = 0.82
    return m

WOOD = mat('M_Wood', (0.28, 0.11, 0.035))
WOOD_LIGHT = mat('M_Driftwood', (0.38, 0.25, 0.13))
LEAF = mat('M_Leaf', (0.035, 0.34, 0.07))
LEAF_DARK = mat('M_LeafDark', (0.02, 0.20, 0.035))
ROCK = mat('M_Rock', (0.25, 0.28, 0.24))
ROCK_DARK = mat('M_RockDark', (0.15, 0.17, 0.15))

def activate_collection(name):
    col = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(col)
    bpy.context.view_layer.active_layer_collection = bpy.context.view_layer.layer_collection.children[col.name]
    return col

def add_cylinder(radius, depth, loc, material, vertices=8, rot=(0,0,0)):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=loc, rotation=rot)
    obj = bpy.context.object
    obj.data.materials.append(material)
    return obj

def add_ico(radius, loc, scale, material, subdivisions=1):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=subdivisions, radius=radius, location=loc)
    obj = bpy.context.object
    obj.scale = scale
    obj.data.materials.append(material)
    return obj

def join_collection(col, name):
    bpy.ops.object.select_all(action='DESELECT')
    meshes = [o for o in col.objects if o.type == 'MESH']
    for o in meshes: o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.join()
    obj = bpy.context.object
    obj.name = name
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    return obj

def export_obj(obj, filename):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.gltf(filepath=os.path.join(ROOT, filename), use_selection=True,
        export_format='GLB', export_apply=True, export_animations=False)

# Palm tree
col = activate_collection('PalmTree')
for i in range(5):
    add_cylinder(0.34 - i*0.035, 1.8, (i*0.10, 0, 0.9+i*1.65), WOOD, 8, (0, math.radians(-4), 0))
for i in range(7):
    ang = i * math.tau / 7
    leaf = add_ico(1, (0.45+math.cos(ang)*1.15, math.sin(ang)*1.15, 8.45), (1.75, .34, .12), LEAF if i%2 else LEAF_DARK)
    leaf.rotation_euler = (0, math.radians(15), ang)
for i in range(3):
    add_ico(.34, (.35+math.cos(i*2.1)*.35, math.sin(i*2.1)*.35, 8.05), (1,1,1), WOOD, 1)
palm = join_collection(col, 'SM_PalmTree_A')
export_obj(palm, 'SM_PalmTree_A.glb')

# Rock cluster
col = activate_collection('RockCluster')
random.seed(12)
for i in range(6):
    rock = add_ico(1, (random.uniform(-1.3,1.3), random.uniform(-1,1), random.uniform(.35,.75)),
        (random.uniform(.65,1.5), random.uniform(.55,1.25), random.uniform(.45,1.1)), ROCK if i%2 else ROCK_DARK)
    rock.rotation_euler = (random.random(), random.random(), random.random())
rocks = join_collection(col, 'SM_RockCluster_A')
export_obj(rocks, 'SM_RockCluster_A.glb')

# Bush
col = activate_collection('Bush')
random.seed(24)
for i in range(9):
    add_ico(1, (random.uniform(-1.1,1.1), random.uniform(-.9,.9), random.uniform(.45,1.25)),
        (random.uniform(.55,1.0), random.uniform(.55,1.0), random.uniform(.5,.95)), LEAF if i%3 else LEAF_DARK)
bush = join_collection(col, 'SM_Bush_A')
export_obj(bush, 'SM_Bush_A.glb')

# Driftwood
col = activate_collection('Driftwood')
add_cylinder(.42, 4.6, (0,0,.48), WOOD_LIGHT, 7, (0, math.radians(90), 0))
add_cylinder(.17, 2.1, (1.2,.35,.85), WOOD_LIGHT, 7, (math.radians(60), math.radians(72), 0))
add_cylinder(.14, 1.7, (-1.4,-.25,.7), WOOD_LIGHT, 7, (math.radians(-55), math.radians(78), 0))
drift = join_collection(col, 'SM_Driftwood_A')
export_obj(drift, 'SM_Driftwood_A.glb')

# Arrange the four source meshes for a quick visual review render.
palm.location = (-4.5, 2.2, 0)
rocks.location = (0.0, 2.0, 0)
bush.location = (4.0, 2.0, 0)
drift.location = (1.0, -3.0, 0)
bpy.ops.mesh.primitive_plane_add(size=30, location=(0,0,-.04))
ground = bpy.context.object
ground.data.materials.append(mat('M_SandPreview', (0.58, 0.42, 0.20)))
bpy.ops.object.light_add(type='SUN', location=(0,0,12))
bpy.context.object.rotation_euler = (math.radians(28), math.radians(-18), math.radians(28))
bpy.context.object.data.energy = 3.0
bpy.ops.object.camera_add(location=(15,-20,11))
camera = bpy.context.object
camera.rotation_euler = ((Vector((0,0,3.2)) - camera.location).to_track_quat('-Z','Y').to_euler())
bpy.context.scene.camera = camera
bpy.context.scene.render.engine = 'BLENDER_EEVEE'
bpy.context.scene.render.resolution_x = 1000
bpy.context.scene.render.resolution_y = 650
bpy.context.scene.render.resolution_percentage = 100
bpy.context.scene.render.filepath = os.path.join(ROOT, 'IslandAssets_Preview.png')
bpy.context.scene.world.color = (0.12, 0.20, 0.32)
bpy.ops.render.render(write_still=True)

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ROOT, 'IslandAssets_Source.blend'))
print('TREASURE_ISLAND_ASSETS_CREATED', ROOT)
