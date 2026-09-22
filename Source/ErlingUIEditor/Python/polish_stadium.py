"""Targeted stadium finishing pass; preserve user actors, gameplay and existing ink."""
import unreal as u
from pathlib import Path
ROOT='/Game/Erling/ArtDirection'
OUT=Path(u.Paths.project_dir()).resolve().parents[1]/'Temporary/Workspace/outputs/stadium_polish'
A=u.EditorAssetLibrary;M=u.MaterialEditingLibrary
T=u.AssetToolsHelpers.get_asset_tools()
E=u.get_editor_subsystem(u.EditorActorSubsystem)
L=u.get_editor_subsystem(u.LevelEditorSubsystem)
u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')

def material(name,color,two_sided=False):
    mat=u.load_asset(ROOT+'/'+name) or T.create_asset(name,ROOT,u.Material,u.MaterialFactoryNew())
    M.delete_all_material_expressions(mat)
    mat.set_editor_property('two_sided',two_sided)
    rgb=M.create_material_expression(mat,u.MaterialExpressionConstant3Vector)
    rgb.set_editor_property('constant',u.LinearColor(*color,1))
    M.connect_material_property(rgb,'',u.MaterialProperty.MP_BASE_COLOR)
    rough=M.create_material_expression(mat,u.MaterialExpressionConstant);rough.set_editor_property('r',.85)
    M.connect_material_property(rough,'',u.MaterialProperty.MP_ROUGHNESS)
    M.recompile_material(mat);A.save_loaded_asset(mat)
    return mat

seats=[material('M_SeatBlue',(.10,.17,.25)),material('M_SeatLight',(.20,.28,.37)),u.load_asset(ROOT+'/M_ClubPink')]
grass=[material('M_GrassBladeDark',(.14,.21,.034),True),material('M_GrassBladeMid',(.21,.29,.057),True),material('M_GrassBladeLight',(.28,.35,.081),True)]
for name,mats in [('SM_StadiumSeats',seats),('SM_PitchFringe',grass)]:
    task=u.AssetImportTask();task.filename=str(OUT/'meshes'/(name+'.fbx'))
    task.destination_path=ROOT;task.destination_name=name
    task.automated=True;task.replace_existing=True;task.save=True;task.factory=u.FbxFactory()
    opt=u.FbxImportUI();opt.automated_import_should_detect_type=False
    opt.mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH
    opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False
    opt.static_mesh_import_data.set_editor_property('combine_meshes',True)
    opt.static_mesh_import_data.set_editor_property('auto_generate_collision',False)
    task.options=opt;T.import_asset_tasks([task])
    mesh=u.load_asset(ROOT+'/'+name);assert mesh
    for i,mat in enumerate(mats):mesh.set_material(i,mat)
    A.save_loaded_asset(mesh)

assert L.load_level('/Game/Maps/Pitch_ArtDirection')
for actor in list(E.get_all_level_actors()):
    label=actor.get_actor_label()
    if label in ['AD Seat band','AD Detailed seats','AD Grass fringe','AD End aisle'] or label.startswith('AD Sector '):
        E.destroy_actor(actor);continue
    if label=='AD Perimeter wall':
        p=actor.get_actor_location();p.z=100;actor.set_actor_location(p,False,False)
        scale=actor.get_actor_scale3d();scale.z=2;actor.set_actor_scale3d(scale)
    if label=='AD End wall':
        p=actor.get_actor_location();p.z=110;actor.set_actor_location(p,False,False)
        scale=actor.get_actor_scale3d();scale.z=2.2;actor.set_actor_scale3d(scale)
    if label=='AD End seating':actor.static_mesh_component.set_material(0,u.load_asset(ROOT+'/M_Concrete'))
    if label=='AD Club banner':
        p=actor.get_actor_location();p.z=120;actor.set_actor_location(p,False,False)
        scale=actor.get_actor_scale3d();scale.z=1.4;actor.set_actor_scale3d(scale)
    if label=='AD Club lettering':
        p=actor.get_actor_location();p.z=155;actor.set_actor_location(p,False,False)
    if isinstance(actor,u.DirectionalLight):
        actor.light_component.set_editor_property('intensity',4.3)
        actor.light_component.set_editor_property('light_source_angle',3.0)
        actor.light_component.set_editor_property('light_color',u.Color(255,232,201,255))
    if isinstance(actor,u.SkyLight):actor.light_component.set_editor_property('intensity',2.7)
    if isinstance(actor,u.PostProcessVolume) and label=='AD Color and exposure':
        settings=actor.get_editor_property('settings')
        settings.set_editor_property('ambient_occlusion_intensity',.4)
        actor.set_editor_property('settings',settings)

for label,name in [('Detailed seats','SM_StadiumSeats'),('Grass fringe','SM_PitchFringe')]:
    actor=E.spawn_actor_from_class(u.StaticMeshActor,u.Vector(0,0,0))
    actor.set_actor_label('AD '+label);actor.set_folder_path('Art Direction/Details')
    actor.static_mesh_component.set_static_mesh(u.load_asset(ROOT+'/'+name))
    actor.set_actor_enable_collision(False)
    actor.static_mesh_component.set_collision_profile_name('NoCollision')
    # Small grass should not generate thousands of hard speckled shadows.
    if name=='SM_PitchFringe':actor.static_mesh_component.set_cast_shadow(False)

# Sector numbers make seating blocks and the preserved stair aisles legible.
for side in [-1,1]:
    for row in range(6):
        for gap in range(4):
            for step in range(2):
                actor=E.spawn_actor_from_class(u.StaticMeshActor,u.Vector(side*(3377.5+row*90+step*45),-1230+gap*820,(290+row*60)*.65+17.875+step*19.5))
                actor.set_actor_label('AD End aisle');actor.set_folder_path('Art Direction/Details')
                actor.static_mesh_component.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'))
                actor.static_mesh_component.set_material(0,u.load_asset(ROOT+'/M_Concrete'))
                actor.set_actor_scale3d(u.Vector(.45,1.2,.195))
                actor.set_actor_enable_collision(False)
                actor.static_mesh_component.set_collision_profile_name('NoCollision')

for side in [-1,1]:
    for sector in range(8):
        actor=E.spawn_actor_from_class(u.TextRenderActor,u.Vector(-2800+sector*800,side*2790,505),u.Rotator(pitch=0,yaw=-side*90,roll=0))
        actor.set_actor_label('AD Sector '+str(side)+' '+str(sector))
        actor.set_folder_path('Art Direction/Branding')
        actor.text_render.set_text('%02d'%(sector+1))
        actor.text_render.set_world_size(40)
        actor.text_render.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER)
        actor.text_render.set_text_render_color(u.Color(214,225,233,255))

sky=u.load_asset(ROOT+'/M_Sky')
cloud=M.get_material_property_input_node(sky,u.MaterialProperty.MP_EMISSIVE_COLOR)
assert isinstance(cloud,u.MaterialExpressionCustom)
cloud.set_editor_property('code','''
float3 d=normalize(P);
float h=saturate(d.z*1.35);
float3 blue=lerp(float3(.38,.56,.76),Tint,sqrt(h));
float2 q=d.xy/max(.22,d.z+.38);
float cloud=0;float shade=0;
for(int i=0;i<9;i++){
 float a=i*2.39996;float2 c=float2(cos(a),sin(a))*(1.1+(i%3)*.45);
 float shape=-10;
 for(int j=0;j<5;j++){
  float2 centre=c+float2((j-2)*.20,.10*sin(j*2.3+i));
  float2 v=(q-centre)/float2(.27+.03*sin(i+j),.16+.04*cos(j));
  shape=max(shape,1-dot(v,v));
 }
 float stroke=.035*sin(q.x*31+sin(q.y*22))*sin(q.y*29);
 float alpha=smoothstep(-.06,.17,shape+stroke);
 cloud=max(cloud,alpha);
 shade=max(shade,alpha*saturate(.45+(q.y-c.y)*1.1));
}
cloud*=smoothstep(-.03,.12,d.z);
float3 paint=lerp(float3(.63,.70,.79),float3(.95,.89,.79),shade);
return lerp(blue,paint,cloud*.88);
''')
M.recompile_material(sky);A.save_loaded_asset(sky)
assert L.save_current_level()
u.log('STADIUM_POLISH_COMPLETE')
