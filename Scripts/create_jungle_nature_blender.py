import bpy, math, os
from mathutils import Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "SourceAssets", "JungleNature"))
os.makedirs(ROOT, exist_ok=True)
bpy.ops.object.select_all(action="SELECT"); bpy.ops.object.delete(use_global=False)

def mat(name, color):
    m=bpy.data.materials.new(name); m.diffuse_color=(*color,1); m.use_nodes=True
    m.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value=(*color,1)
    m.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value=.9
    return m

BARK=mat("M_JungleBark",(.16,.065,.02)); BARK2=mat("M_JungleBarkLight",(.28,.12,.035))
LEAF=mat("M_JungleLeaf",(.035,.26,.045)); LEAF2=mat("M_JungleLeafDark",(.018,.12,.028))
FERN=mat("M_JungleFern",(.055,.34,.065)); MOSS=mat("M_JungleMoss",(.07,.24,.035))

def coll(n):
    c=bpy.data.collections.new(n); bpy.context.scene.collection.children.link(c)
    bpy.context.view_layer.active_layer_collection=bpy.context.view_layer.layer_collection.children[c.name]; return c
def cube(n,l,s,m,r=(0,0,0)):
    bpy.ops.mesh.primitive_cube_add(size=1,location=l,rotation=r); o=bpy.context.object;o.name=n;o.scale=s;o.data.materials.append(m);return o
def cyl(n,l,rad,dep,m,verts=9,r=(0,0,0)):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts,radius=rad,depth=dep,location=l,rotation=r);o=bpy.context.object;o.name=n;o.data.materials.append(m);return o
def ico(n,l,rad,s,m):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=rad,location=l);o=bpy.context.object;o.name=n;o.scale=s;o.data.materials.append(m);return o
def export(g,n):
    bpy.ops.object.select_all(action="DESELECT"); obs=[o for o in g.objects if o.type=="MESH"]
    for o in obs:o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.select_set(False)
    for o in obs:o.select_set(True)
    bpy.context.view_layer.objects.active=obs[0];bpy.ops.object.join();o=bpy.context.object;o.name=n
    bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    bpy.ops.object.select_all(action="DESELECT");o.select_set(True);bpy.context.view_layer.objects.active=o
    bpy.ops.export_scene.gltf(filepath=os.path.join(ROOT,n+".glb"),use_selection=True,export_format="GLB",export_apply=True,export_animations=False);return o

# Broad buttress roots make this a drawable landmark rather than a generic tree.
g=coll("ButtressTree")
cyl("Trunk",(0,0,3.8),.55,7.6,BARK,10)
for a in range(0,360,60):
    ang=math.radians(a); cube("Buttress",(math.cos(ang)*.9,math.sin(ang)*.9,.65),(1.35,.22,.58),BARK2,(0,0,ang))
for p in ((0,0,7.7),(-1.5,.3,7.1),(1.35,.55,7.25),(0,-1.45,7.0)):
    ico("Crown",p,1.7,(1.4,1.1,.75),LEAF if p[0]>=0 else LEAF2)
buttress=export(g,"SM_ButtressTree_A")

g=coll("ForkedTree")
cyl("LowerTrunk",(0,0,2.7),.42,5.4,BARK,9)
cyl("LeftFork",(-.85,0,5.7),.30,4.0,BARK2,9,(0,math.radians(-25),0))
cyl("RightFork",(.9,.15,5.6),.28,3.8,BARK,9,(0,math.radians(28),0))
for p in ((-1.7,0,7.3),(1.7,.2,7.2),(0,.4,6.7)):
    ico("Crown",p,1.6,(1.25,1.0,.75),LEAF2 if p[0]<0 else LEAF)
forked=export(g,"SM_ForkedJungleTree_A")

g=coll("FernCluster")
for i in range(11):
    a=i*math.tau/11; length=1.0+(i%3)*.18
    cube("FernFrond",(math.cos(a)*.45,math.sin(a)*.45,.35),(length,.11,.045),FERN,(0,math.radians(-12),a))
fern=export(g,"SM_FernCluster_A")

g=coll("JungleBush")
for p,s in (((0,0,.65),1.0),((.8,.15,.55),.72),((-.75,-.1,.5),.68),((.1,.72,.48),.62)):
    ico("BushLeaf",p,.9,(s,s*.8,s*.65),LEAF if p[0]>=0 else LEAF2)
bush=export(g,"SM_JungleBush_A")

g=coll("FallenLog")
cyl("Log",(0,0,.55),.48,5.2,BARK,10,(0,math.radians(90),0))
for x in (-2.2,2.1):cyl("BrokenEnd",(x,0,.55),.58,.18,BARK2,9,(0,math.radians(90),0))
for x in (-1.4,.2,1.35):cube("Moss",(x,-.4,.82),(.55,.12,.12),MOSS,(0,0,math.radians((x+1)*13)))
log=export(g,"SM_FallenJungleLog_A")

for o,p in {buttress:(-7,2.5,0),forked:(-2,2.5,0),fern:(3,2.5,0),bush:(6,2.5,0),log:(1,-4,0)}.items():o.location=p
bpy.ops.mesh.primitive_plane_add(size=25,location=(0,0,-.05));bpy.context.object.data.materials.append(mat("M_JungleGround",(.10,.20,.055)))
bpy.ops.object.light_add(type="SUN",location=(0,0,15));bpy.context.object.rotation_euler=(math.radians(30),math.radians(-18),math.radians(25));bpy.context.object.data.energy=3.2
bpy.ops.object.camera_add(location=(19,-25,15));cam=bpy.context.object;cam.rotation_euler=((Vector((0,0,2.8))-cam.location).to_track_quat("-Z","Y").to_euler());bpy.context.scene.camera=cam
bpy.context.scene.render.engine="BLENDER_EEVEE";bpy.context.scene.render.resolution_x=1280;bpy.context.scene.render.resolution_y=800;bpy.context.scene.render.resolution_percentage=100
bpy.context.scene.render.filepath=os.path.join(ROOT,"JungleNature_Preview.png");bpy.context.scene.render.image_settings.file_format="PNG";bpy.context.scene.world.color=(.035,.07,.03)
bpy.ops.render.render(write_still=True);bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ROOT,"JungleNature_Source.blend"))
print("TREASURE_JUNGLE_NATURE_CREATED",ROOT)
