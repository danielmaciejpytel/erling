"""Blender: locally increase collar clearance, preserving topology/UVs/morph deltas.
Run once on the archived pre-fix rig, with -- <output blend> <output mesh FBX>.
"""
import bpy, sys, json, numpy as np
from mathutils import Vector

args=sys.argv[sys.argv.index('--')+1:]
shirt=bpy.data.objects['shirt'];rig=bpy.data.objects['Erling_Rig']
assert not shirt.get('collar_clearance_v1'), 'Collar adjustment already applied'
def smooth(a,b,x):
    t=max(0,min(1,(x-a)/(b-a)))
    return t*t*(3-2*t)

offsets=[]
for vertex in shirt.data.vertices:
    p=vertex.co
    weight=smooth(.94,1.08,p.z)*(1-smooth(.43,.56,abs(p.x)))
    # Radial clearance avoids raising the neck opening or changing the sleeves.
    direction=Vector((p.x,p.y+.045,0)).normalized()
    offsets.append(direction*(.008*weight))
keys=shirt.data.shape_keys.key_blocks
delta=np.asarray([tuple(v) for v in offsets],dtype=np.float32).reshape(-1)
points=np.empty(len(offsets)*3,dtype=np.float32)
for key in keys:
    key.data.foreach_get('co',points)
    key.data.foreach_set('co',points+delta)
keys[0].data.foreach_get('co',points)
shirt.data.vertices.foreach_set('co',points)
shirt['collar_clearance_v1']=True
shirt.data.update()
bpy.ops.wm.save_as_mainfile(filepath=args[0],relative_remap=False)
# Export rest geometry and all existing corrective names, not a posed frame.
rig.data.pose_position='REST'
bpy.ops.object.select_all(action='DESELECT')
shirt.select_set(True);rig.select_set(True)
bpy.context.view_layer.objects.active=rig
bpy.ops.export_scene.fbx(filepath=args[1],use_selection=True,
    object_types={'MESH','ARMATURE'},use_mesh_modifiers=False,
    add_leaf_bones=False,bake_anim=False,axis_forward='-Z',axis_up='Y')
print('COLLAR_FIXED',json.dumps({'vertices':len(shirt.data.vertices),
    'adjusted_vertices':sum(d.length>0 for d in offsets),
    'maximum_offset_m':max(d.length for d in offsets),
    'shape_keys_including_basis':len(keys),'bones':len(rig.data.bones)}),flush=True)
