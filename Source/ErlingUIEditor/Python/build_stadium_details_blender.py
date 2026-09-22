"""Blender source for economical, merged stadium seats and grass clumps.
Coordinates below are Unreal centimetres. Run --factory-startup --background --python
this file -- <output folder>. No scene or character assets are read or modified.
"""
import bpy,sys,random,math
from pathlib import Path
out=Path(sys.argv[sys.argv.index('--')+1]);out.mkdir(parents=True,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
rng=random.Random(220926)
vertices=[];faces=[];slots=[]
def coord(p):return (p[0]/100,-p[1]/100,p[2]/100)
def poly(points,material=0):
    start=len(vertices);vertices.extend(coord(p) for p in points)
    faces.append(tuple(reversed(range(start,start+len(points)))));slots.append(material)
def box(center,size,material=0,turn=False):
    x,y,z=center;w,d,h=size
    pts=[(sx*w/2,sy*d/2,sz*h/2) for sz in [-1,1] for sy in [-1,1] for sx in [-1,1]]
    pts=[(x+(b if turn else a),y+(a if turn else b),z+c) for a,b,c in pts]
    for indices in [(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)]:poly([pts[i] for i in indices],material)
def export(name,colors):
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(vertices,[],faces);mesh.update()
    obj=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(obj)
    for color in colors:
        mat=bpy.data.materials.new('Detail_'+str(len(mesh.materials)));mat.diffuse_color=(*color,1);mesh.materials.append(mat)
    for p,slot in zip(mesh.polygons,slots):p.material_index=slot
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
    bpy.ops.export_scene.fbx(filepath=str(out/(name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',bake_anim=False,add_leaf_bones=False)
    print('DETAIL_MESH',name,'vertices',len(vertices),'faces',len(faces),flush=True)
    vertices.clear();faces.clear();slots.clear()

for side in [-1,1]:
    for row in range(8):
        y=side*(2150+row*90);z=(240+row*58)*.65+35
        for sector in range(8):
            for chair in range(13):
                x=-2800+sector*800+(chair-6)*49
                color=2 if (sector in [2,5] and chair in [5,6,7]) else (row+chair//3)%2
                box((x,y-side*10,z),(40,38,6),color)
                box((x,y+side*9,z+19),(40,6,35),color)
    for row in range(6):
        x=side*(3400+row*90);z=(290+row*60)*.65+32
        for sector in range(5):
            for chair in range(13):
                y=-1640+sector*820+(chair-6)*49
                color=2 if sector==2 else (row+chair//3)%2
                box((x-side*10,y,z),(40,38,6),color,True)
                box((x+side*9,y,z+19),(40,6,35),color,True)
export('SM_StadiumSeats',[(.17,.24,.32),(.24,.31,.40),(.65,.025,.18)])

for i in range(2400):
    side=rng.choice([-1,1])
    if i<1700:x=rng.uniform(-3100,3100);y=side*rng.uniform(1840,1980)
    else:x=side*rng.uniform(3060,3200);y=rng.uniform(-1840,1840)
    # Six bent blades, each three triangles, per clump. No collision.
    for leaf in range(6):
        a=rng.random()*math.tau;h=rng.uniform(4,10);w=rng.uniform(.8,1.8)
        dx,dy=math.cos(a),math.sin(a);bend=rng.uniform(1,4)
        root=(x+rng.uniform(-3,3),y+rng.uniform(-3,3),.7)
        left=(root[0]-dy*w,root[1]+dx*w,root[2]);right=(root[0]+dy*w,root[1]-dx*w,root[2])
        ml=(root[0]+dx*bend-dy*w*.5,root[1]+dy*bend+dx*w*.5,h*.6)
        mr=(root[0]+dx*bend+dy*w*.5,root[1]+dy*bend-dx*w*.5,h*.6)
        tip=(root[0]+dx*bend*2,root[1]+dy*bend*2,h)
        slot=rng.randrange(3)
        poly([left,right,mr],slot);poly([left,mr,ml],slot);poly([ml,mr,tip],slot)
export('SM_PitchFringe',[(.16,.24,.04),(.24,.32,.07),(.31,.38,.10)])
