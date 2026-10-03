import bpy, math
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceAssets'/'CanyonNewProps'
OUT.mkdir(parents=True,exist_ok=True)

def material(name,color,metal=0):
    m=bpy.data.materials.new(name); m.diffuse_color=(*color,1); m.use_nodes=True
    bs=m.node_tree.nodes.get('Principled BSDF'); bs.inputs['Base Color'].default_value=(*color,1); bs.inputs['Metallic'].default_value=metal; bs.inputs['Roughness'].default_value=.65
    return m
def cube(name,loc,scale,mat):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc); o=bpy.context.object; o.name=name; o.scale=scale; o.data.materials.append(mat); return o
def cylinder(name,loc,radius,depth,mat,vertices=12,rotation=None):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=radius,depth=depth,location=loc); o=bpy.context.object; o.name=name; o.data.materials.append(mat)
    if rotation: o.rotation_euler=rotation
    return o
def rock(name,loc,scale,mat):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=1,location=loc); o=bpy.context.object; o.name=name; o.scale=scale; o.rotation_euler=(.1,.2,.3); o.data.materials.append(mat); return o
def beam(name,a,b,width,mat):
    a,b=Vector(a),Vector(b); o=cube(name,(a+b)/2,(width,width,(b-a).length),mat); o.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler(); return o

for kind in ['01_SupplyCrate','02_WaterBarrel','03_MineCart','04_Cactus','05_OreCluster']:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    wood=material('Warm weathered wood',(.34,.16,.065)); light=material('Wood edge',(.53,.29,.12)); iron=material('Dark iron',(.12,.15,.18),.65)
    if kind=='01_SupplyCrate':
        cube('Crate body',(0,0,.48),(.88,.78,.9),wood)
        for z in [.12,.84]:
            for y in [-.405,.405]: cube('Horizontal frame',(0,y,z),(.98,.07,.12),light)
        for x in [-.43,.43]:
            for y in [-.405,.405]: cube('Corner frame',(x,y,.48),(.12,.07,.86),light)
        for y in [-.45,.45]: beam('Diagonal brace',(-.35,y,.2),(.35,y,.76),.085,light)
        for x in [-.25,0,.25]: cube('Lid plank',(x,0,.965),(.235,.8,.055),light)
    elif kind=='02_WaterBarrel':
        verts=[]; faces=[]; n=14
        for z,r in [(0,.32),(.12,.38),(.48,.43),(.86,.38),(.98,.32)]:
            verts.extend([(r*math.cos(i*2*math.pi/n),r*math.sin(i*2*math.pi/n),z) for i in range(n)])
        for j in range(4):
            for i in range(n): faces.append((j*n+i,j*n+(i+1)%n,(j+1)*n+(i+1)%n,(j+1)*n+i))
        faces.extend([tuple(range(n-1,-1,-1)),tuple(range(4*n,5*n))]); mesh=bpy.data.meshes.new('Barrel staves');mesh.from_pydata(verts,[],faces);obj=bpy.data.objects.new('Barrel',mesh);bpy.context.collection.objects.link(obj);obj.data.materials.append(wood)
        for z,r in [(.15,.39),(.48,.439),(.82,.395)]:
            bpy.ops.mesh.primitive_torus_add(major_segments=14,minor_segments=4,location=(0,0,z),major_radius=r,minor_radius=.025);bpy.context.object.data.materials.append(iron)
        cylinder('Lid',(0,0,.99),.32,.035,light,14)
        cylinder('Bung',(.1,.05,1.018),.055,.025,wood,8)
    elif kind=='03_MineCart':
        cube('Cart bottom',(0,0,.42),(1.25,.8,.12),iron)
        for y in [-.42,.42]: cube('Side panels',(0,y,.76),(1.32,.085,.62),wood)
        for x in [-.66,.66]: cube('End panels',(x,0,.76),(.085,.84,.62),wood)
        for y in [-.46,.46]: cube('Upper rim',(0,y,1.09),(1.44,.075,.07),iron)
        for x in [-.7,.7]: cube('End rim',(x,0,1.09),(.075,.95,.07),iron)
        for x in [-.43,.43]:
            cylinder('Axle',(x,0,.23),.045,1.05,iron,10,(math.pi/2,0,0))
            for y in [-.5,.5]: cylinder('Wheel',(x,y,.23),.22,.1,iron,12,(math.pi/2,0,0))
        ore=material('Ore rock',(.3,.28,.25))
        for x,y,z in [(-.3,0,.67),(.3,.15,.7),(.05,-.15,.75)]:rock('Cart ore',(x,y,z),(.3,.25,.22),ore)
    elif kind=='04_Cactus':
        green=material('Cactus green',(.18,.39,.22)); tip=material('Cactus ridges',(.31,.51,.25)); flower=material('Coral flower',(.9,.34,.22))
        cylinder('Main stem',(0,0,.82),.2,1.64,green,10);rock('Rounded cap',(0,0,1.64),(.2,.2,.16),green)
        beam('Left arm',(-.02,0,.7),(-.47,0,.7),.22,green);cylinder('Left upright',(-.47,0,.95),.13,.5,green,8);rock('Left cap',(-.47,0,1.2),(.13,.13,.12),green)
        beam('Right arm',(.02,0,1.04),(.4,0,1.04),.2,green);cylinder('Right upright',(.4,0,1.25),.115,.42,green,8);rock('Right cap',(.4,0,1.46),(.115,.115,.1),green)
        for z in [.35,.65,.95,1.25]: cube('Spine marks',(.04,-.202,z),(.025,.015,.045),tip)
        for i in range(5):rock('Flower petal',(.09*math.cos(i*1.256),.09*math.sin(i*1.256),1.77),(.075,.075,.04),flower)
    else:
        stone=material('Sandstone ore base',(.45,.29,.18)); copper=material('Copper crystal',(.23,.58,.55),.25)
        for x,y,z,s in [(-.32,0,.23,.35),(.25,.1,.22,.34),(0,-.2,.18,.28)]:rock('Ore stone',(x,y,z),(s,s*.8,s*.75),stone)
        for x,y,z,r,h in [(-.2,0,.55,.12,.7),(.12,.05,.48,.14,.6),(.3,-.08,.4,.09,.43)]:
            bpy.ops.mesh.primitive_cone_add(vertices=5,radius1=r,radius2=0,depth=h,location=(x,y,z));bpy.context.object.name='Copper crystal';bpy.context.object.data.materials.append(copper)
    props=list(bpy.context.scene.objects)
    bpy.context.view_layer.update(); pts=[o.matrix_world@Vector(c) for o in props for c in o.bound_box]
    lo=Vector(tuple(min(p[i] for p in pts) for i in range(3)));hi=Vector(tuple(max(p[i] for p in pts) for i in range(3)));center=(lo+hi)/2;size=max(hi-lo)
    bpy.ops.object.select_all(action='DESELECT')
    for o in props:o.select_set(True)
    bpy.ops.export_scene.gltf(filepath=str(OUT/(kind+'.glb')),use_selection=True,export_format='GLB')
    floor=material('Studio ground',(.22,.24,.27));cube('Display ground',(center.x,center.y,-.045),(200,200,.05),floor)
    scene=bpy.context.scene;scene.world=bpy.data.worlds.new('Studio');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.12,.14,.18,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.4
    for offset,power in [((-2,-3,5),900),((3,1,3),650)]:
        bpy.ops.object.light_add(type='AREA',location=center+Vector(offset)*size);o=bpy.context.object;o.data.energy=power*size*size;o.data.size=size*3;o.rotation_euler=(center-o.location).to_track_quat('-Z','Y').to_euler()
    bpy.ops.object.camera_add(location=center+Vector((1.6,-2.3,1.3))*size);o=bpy.context.object;o.rotation_euler=(center-o.location).to_track_quat('-Z','Y').to_euler();o.data.type='ORTHO';o.data.ortho_scale=size*1.5;scene.camera=o
    scene.render.engine='BLENDER_EEVEE';scene.render.resolution_x=800;scene.render.resolution_y=640;scene.render.resolution_percentage=100;scene.render.filepath=str(OUT/(kind+'.png'));bpy.ops.render.render(write_still=True);bpy.ops.wm.save_as_mainfile(filepath=str(OUT/(kind+'.blend')))
print('FIVE_NEW_CANYON_PROPS_COMPLETE')
