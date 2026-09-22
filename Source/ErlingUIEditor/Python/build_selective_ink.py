"""One-pixel visible silhouette ink on runtime stencil 1, preserving scene layout."""
import unreal as u

M=u.MaterialEditingLibrary
A=u.EditorAssetLibrary
root='/Game/Erling/ArtDirection'
mat=u.load_asset(root+'/M_SelectiveInk') or u.AssetToolsHelpers.get_asset_tools().create_asset('M_SelectiveInk',root,u.Material,u.MaterialFactoryNew())
M.delete_all_material_expressions(mat)
mat.set_editor_property('material_domain',u.MaterialDomain.MD_POST_PROCESS)
mat.set_editor_property('blendable_location',u.BlendableLocation.BL_SCENE_COLOR_BEFORE_DOF)
scene=M.create_material_expression(mat,u.MaterialExpressionSceneTexture)
scene.set_editor_property('scene_texture_id',u.SceneTextureId.PPI_POST_PROCESS_INPUT0)
effect=M.create_material_expression(mat,u.MaterialExpressionCustom)
effect.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT3)
value=u.CustomInput();value.set_editor_property('input_name','Scene')
effect.set_editor_property('inputs',[value])
effect.set_editor_property('code','''
float2 uv=GetDefaultSceneTextureUV(Parameters,13);
float2 stepUV=View.BufferSizeAndInvSize.zw;
float cd=SceneTextureLookup(uv,13,false).r;
float sd=SceneTextureLookup(uv,1,false).r;
float mask=SceneTextureLookup(uv,25,false).r;
float edge=0;
float2 offsets[4]={float2(1,0),float2(-1,0),float2(0,1),float2(0,-1)};
for(int i=0;i<4;i++){
 float2 p=uv+offsets[i]*stepUV;
 float other=SceneTextureLookup(p,25,false).r;
 edge=max(edge,abs(mask-other));
}
float visible=(mask>.5 && mask<1.5 && cd<=sd+2)?1:0;
return lerp(Scene.rgb,float3(.025,.029,.041),saturate(edge)*visible*.72);
''')
M.connect_material_expressions(scene,'Color',effect,'Scene')
M.connect_material_property(effect,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
M.recompile_material(mat);A.save_loaded_asset(mat)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert levels.load_level('/Game/Maps/Pitch_ArtDirection')
volumes=[a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors() if isinstance(a,u.PostProcessVolume) and a.get_actor_label()=='AD Color and exposure']
assert len(volumes)==1
volumes[0].add_or_update_blendable(mat,1.0)
levels.save_current_level()
u.log('SELECTIVE_INK_COMPLETE')
