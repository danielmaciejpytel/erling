"""Build the reversible art-direction map. Run with Unreal Editor Python.

Pitch and its shared materials are never edited. Re-running updates only assets
owned by this script; custom additions to the new map are retained.
"""
import math
import unreal as u

A = u.EditorAssetLibrary
M = u.MaterialEditingLibrary
T = u.AssetToolsHelpers.get_asset_tools()
E = u.get_editor_subsystem(u.EditorActorSubsystem)
L = u.get_editor_subsystem(u.LevelEditorSubsystem)
ROOT = '/Game/Erling/ArtDirection'
MAP = '/Game/Maps/Pitch_ArtDirection'

def node(mat, cls, **props):
    n = M.create_material_expression(mat, cls)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n

def constant(mat, value):
    return node(mat, u.MaterialExpressionConstant, r=value)

def custom_input(name):
    value=u.CustomInput()
    value.set_editor_property('input_name',name)
    return value

def material(name, color, roughness=.85, code=None, sky=False):
    path = ROOT + '/' + name
    mat = u.load_asset(path) or T.create_asset(name, ROOT, u.Material, u.MaterialFactoryNew())
    M.delete_all_material_expressions(mat)
    if sky:
        mat.set_editor_property('two_sided', True)
        mat.set_editor_property('is_sky', True)
        mat.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
    n = node(mat, u.MaterialExpressionConstant3Vector, constant=u.LinearColor(*color, 1))
    if code:
        p = node(mat, u.MaterialExpressionWorldPosition)
        custom = node(mat, u.MaterialExpressionCustom, code=code, output_type=u.CustomMaterialOutputType.CMOT_FLOAT3)
        custom.set_editor_property('inputs', [custom_input('P'), custom_input('Tint')])
        M.connect_material_expressions(p, '', custom, 'P')
        M.connect_material_expressions(n, '', custom, 'Tint')
        n = custom
    M.connect_material_property(n, '', u.MaterialProperty.MP_EMISSIVE_COLOR if sky else u.MaterialProperty.MP_BASE_COLOR)
    M.connect_material_property(constant(mat, roughness), '', u.MaterialProperty.MP_ROUGHNESS)
    M.connect_material_property(constant(mat, .2), '', u.MaterialProperty.MP_SPECULAR)
    M.recompile_material(mat)
    A.save_loaded_asset(mat)
    return mat

grass_code = '''
float2 q=P.xy;
float broad=sin(q.x*.007+sin(q.y*.004))*sin(q.y*.009);
float fleck=sin(q.x*.31+sin(q.y*.23)*2)*sin(q.y*.42+sin(q.x*.17));
float fine=sin(q.x*1.7+q.y*.39)*sin(q.y*1.3);
return Tint*(1+.075*broad+.045*fleck+.015*fine);
'''
grass = [material('M_GrassA', (.19,.27,.055), code=grass_code),
         material('M_GrassB', (.215,.30,.066), code=grass_code)]
navy = material('M_Stadium', (.045,.052,.083))
concrete = material('M_Concrete', (.13,.145,.185))
seat = material('M_Seats', (.18,.21,.28))
pink = material('M_ClubPink', (.64,.025,.20))
white = material('M_Chalk', (.8,.81,.67))
metal = material('M_Metal', (.035,.045,.065), .55)
outside = material('M_Outside', (.075,.105,.033), code=grass_code)
sky = material('M_Sky', (.15,.32,.61), sky=True, code='''
float3 d=normalize(P);
float h=saturate(d.z*1.5);
float3 blue=lerp(float3(.28,.46,.72),Tint,sqrt(h));
float2 q=d.xy/max(.18,d.z+.32);
float cloud=0;
for(int i=0;i<12;i++) {
 float a=i*2.39996; float2 c=float2(cos(a),sin(a))*(1.5+(i%3)*.70);
 float2 v=(q-c)/float2(.52,.23);
 float shape=1-dot(v,v)+.18*sin(q.x*27+i)*sin(q.y*21);
 cloud=max(cloud,smoothstep(0,.32,shape));
}
cloud*=smoothstep(-.08,.09,d.z);
return lerp(blue,float3(.92,.86,.77),cloud*.88);
''')

# Clone the existing character materials, keeping every texture and morph flag.
for name in ['body','hair','shirt','pants','shoe','Face']:
    dest = ROOT + '/M_' + name
    mat = u.load_asset(dest) or A.duplicate_asset('/Game/Erling/Materials/M_'+name, dest)
    if name in ['body','hair']:
        current=M.get_material_property_input_node(mat,u.MaterialProperty.MP_BASE_COLOR)
        if current.get_editor_property('desc')!='Art direction warmth':
            tint=node(mat,u.MaterialExpressionConstant3Vector,constant=u.LinearColor(*( (.98,.77,.68) if name=='body' else (1,.87,.65)),1))
            mult=node(mat,u.MaterialExpressionMultiply,desc='Art direction warmth')
            M.connect_material_expressions(current,'',mult,'A')
            M.connect_material_expressions(tint,'',mult,'B')
            M.connect_material_property(mult,'',u.MaterialProperty.MP_BASE_COLOR)
    for prop,value in [(u.MaterialProperty.MP_EMISSIVE_COLOR,0),(u.MaterialProperty.MP_ROUGHNESS,.82),(u.MaterialProperty.MP_SPECULAR,.22)]:
        current=M.get_material_property_input_node(mat,prop)
        if isinstance(current,u.MaterialExpressionConstant):current.set_editor_property('r',value)
        else:M.connect_material_property(constant(mat,value),'',prop)
    M.recompile_material(mat)
    A.save_loaded_asset(mat)

# Spherical Voronoi panels use the 12 icosahedron vertices and 20 face centres.
phi = (1+math.sqrt(5))/2
vertices = []
for a in [-1,1]:
    for b in [-phi,phi]:
        vertices.extend([(0,a,b),(a,b,0),(b,0,a)])
def norm(v):
    size=math.sqrt(sum(x*x for x in v)); return tuple(x/size for x in v)
centres = [norm(v) for v in vertices]
for i in range(12):
    for j in range(i+1,12):
        for k in range(j+1,12):
            if all(abs(sum((vertices[a][n]-vertices[b][n])**2 for n in range(3))-4)<.001 for a,b in [(i,j),(i,k),(j,k)]):
                centres.append(norm(tuple(vertices[i][n]+vertices[j][n]+vertices[k][n] for n in range(3))))
assert len(centres)==32
ball = material('M_Ball', (.8,.79,.7))
uv=node(ball,u.MaterialExpressionTextureCoordinate)
code='float3 dirs[32]={'+','.join('float3(%s)' % ','.join(str(x) for x in c) for c in centres)+'};\n'
code+='''float a=UV.x*6.2831853; float b=UV.y*3.1415926;
float3 n=float3(cos(a)*sin(b),sin(a)*sin(b),cos(b));
float first=-2,second=-2;int id=0;
for(int i=0;i<32;i++){float d=dot(n,dirs[i]);if(d>first){second=first;first=d;id=i;}else second=max(second,d);}
float edge=smoothstep(.002,.012,first-second);
float3 col=id<12?float3(.045,.055,.082):float3(.78,.76,.67);
return lerp(float3(.10,.11,.12),col,edge);'''
c=node(ball,u.MaterialExpressionCustom,code=code,output_type=u.CustomMaterialOutputType.CMOT_FLOAT3)
c.set_editor_property('inputs',[custom_input('UV')])
M.connect_material_expressions(uv,'',c,'UV')
M.connect_material_property(c,'',u.MaterialProperty.MP_BASE_COLOR)
M.recompile_material(ball);A.save_loaded_asset(ball)

if not A.does_asset_exist(MAP):
    assert A.duplicate_asset('/Game/Maps/Pitch',MAP)
assert L.load_level(MAP)
cube=u.load_asset('/Engine/BasicShapes/Cube')
for actor in list(E.get_all_level_actors()):
    label=actor.get_actor_label()
    if label.startswith('AD ') or label=='Stand':
        E.destroy_actor(actor)
        continue
    if isinstance(actor,u.StaticMeshActor):
        if label.startswith('Grass stripe'):
            actor.static_mesh_component.set_material(0,grass[round((actor.get_actor_location().x+2475)/450)%2])
        elif label=='Pitch foundation':actor.static_mesh_component.set_material(0,outside)
        elif label in ['Touchline','Goal line','Halfway line','Centre circle','Penalty area']:
            actor.static_mesh_component.set_material(0,white)
            actor.static_mesh_component.set_cast_shadow(False)
            location=actor.get_actor_location();location.z=.8
            actor.set_actor_location(location,False,False)
            scale=actor.get_actor_scale3d();scale.z=.003
            actor.set_actor_scale3d(scale)
    if isinstance(actor,u.DirectionalLight):
        actor.set_actor_rotation(u.Rotator(pitch=-32,yaw=132,roll=0),False)
        actor.light_component.set_editor_property('intensity',4.0)
        actor.light_component.set_editor_property('light_color',u.Color(255,231,196,255))
        actor.light_component.set_editor_property('light_source_angle',2.0)
    if isinstance(actor,u.SkyLight):
        actor.light_component.set_editor_property('intensity',2.2)
        actor.light_component.set_editor_property('real_time_capture',False)
        actor.light_component.set_editor_property('source_type',u.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
        actor.light_component.set_editor_property('cubemap',u.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))

def box(label, p, size, mat, yaw=0):
    if label in ['Seating terrace','Seat band','Stair aisle','Upper fascia','Roof lip','End seating','End fascia']:
        p=(p[0],p[1],p[2]*.65)
        size=(size[0],size[1],size[2]*.65)
    if label in ['Floodlight mast','Floodlight housing','Floodlight lens']:
        p=(p[0],p[1],p[2]*.55)
        size=(size[0],size[1],size[2]*.55)
    a=E.spawn_actor_from_class(u.StaticMeshActor,u.Vector(*p),u.Rotator(pitch=0,yaw=yaw,roll=0))
    a.set_actor_label('AD '+label)
    a.set_folder_path('Art Direction/Stadium')
    a.static_mesh_component.set_static_mesh(cube)
    a.static_mesh_component.set_material(0,mat)
    a.set_actor_scale3d(u.Vector(*(v/100 for v in size)))
    a.set_actor_enable_collision(False)
    a.static_mesh_component.set_collision_profile_name('NoCollision')
    return a

# Seating bands, open stairs, fascia and club banners frame all play cameras.
for s in [-1,1]:
    box('Perimeter wall',(0,s*2050,140),(6500,80,280),navy)
    box('Pink touchline trim',(0,s*2005,45),(6450,8,16),pink)
    for row in range(8):
        y=s*(2150+row*90); z=240+row*58
        for sector in range(8):
            x=-2800+sector*800
            box('Seating terrace',(x,y,z),(690,90,80),concrete)
            box('Seat band',(x,y-s*16,z+49),(675,48,22),seat)
        for stair in range(7):
            box('Stair aisle',(-2400+stair*800,y,z+30),(108,90,18),concrete)
    box('Upper fascia',(0,s*2880,785),(6550,160,160),navy)
    box('Roof lip',(0,s*2770,880),(6650,420,36),concrete)
    for x in [-2200,0,2200]:
        box('Club banner',(x,s*1955,170),(300,10,165),pink)
    # End stands preserve the goal mouth and net collision volume.
    box('End wall',(s*3300,0,160),(80,4100,320),navy)
    for row in range(6):
        for sector in range(5):
            box('End seating',(s*(3400+row*90),-1640+sector*820,290+row*60),(90,700,85),seat)
    box('End fascia',(s*3970,0,740),(170,4350,180),navy)
    for y in [-2500,2500]:
        box('Floodlight mast',(s*3050,y,740),(35,35,1480),metal)
        box('Floodlight housing',(s*3050,y,1460),(260,90,190),navy)
        for ix in [-1,0,1]:
            for iz in [-1,1]:
                box('Floodlight lens',(s*3050+ix*72,y-55,1460+iz*46),(51,8,59),white)

for s in [-1,1]:
    for y in range(-350,351,50):
        box('Roof net',(s*2865,y,260),(250,2,2),white)
    for x in range(2740,2991,50):
        box('Roof net',(s*x,0,260),(2,700,2),white)
    for y in [-350,350]:
        for z in range(40,261,40):
            box('Side net',(s*2865,y,z),(250,2,2),white)
        for x in range(2760,2991,45):
            box('Side net',(s*x,y,130),(2,2,260),white)

for s in [-1,1]:
    for x in [-2200,0,2200]:
        text=E.spawn_actor_from_class(u.TextRenderActor,u.Vector(x,s*1946,195),u.Rotator(pitch=0,yaw=-s*90,roll=0))
        text.set_actor_label('AD Club lettering')
        text.set_folder_path('Art Direction/Branding')
        text.text_render.set_text('ERLING')
        text.text_render.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER)
        text.text_render.set_world_size(52)
        text.text_render.set_text_render_color(u.Color(240,233,218,255))

dome=E.spawn_actor_from_class(u.StaticMeshActor,u.Vector(0,0,0))
dome.set_actor_label('AD Sky')
dome.set_folder_path('Art Direction/Lighting')
dome.static_mesh_component.set_static_mesh(u.load_asset('/Engine/BasicShapes/Sphere'))
dome.static_mesh_component.set_material(0,sky)
dome.static_mesh_component.set_cast_shadow(False)
dome.set_actor_scale3d(u.Vector(400,400,400))
dome.set_actor_enable_collision(False)

pp=E.spawn_actor_from_class(u.PostProcessVolume,u.Vector(0,0,0))
pp.set_actor_label('AD Color and exposure');pp.set_editor_property('unbound',True)
settings=pp.get_editor_property('settings')
for key,value in [('auto_exposure_min_brightness',1.0),('auto_exposure_max_brightness',1.0),('auto_exposure_bias',0.0),('bloom_intensity',.12),('vignette_intensity',.18),('ambient_occlusion_intensity',.65),('ambient_occlusion_radius',90.0)]:
    settings.set_editor_property('override_'+key,True)
    settings.set_editor_property(key,value)
pp.set_editor_property('settings',settings)
assert L.save_current_level()
unreal_count=len(E.get_all_level_actors())
u.log('ART_DIRECTION_READY actors=%d map=%s' % (unreal_count,MAP))
