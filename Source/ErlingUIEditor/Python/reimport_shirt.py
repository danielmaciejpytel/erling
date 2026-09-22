"""Reimport only the canonical shirt. Abort if skeleton or corrective names change."""
import unreal as u
from pathlib import Path
A=u.EditorAssetLibrary
path='/Game/Erling/Meshes/SK_shirt'
shirt=u.load_asset(path);assert shirt
skeleton=shirt.skeleton
before={m.get_name() for m in shirt.get_editor_property('morph_targets')}
slots=shirt.get_editor_property('materials')
u.log('SHIRT_REFERENCERS '+str(A.find_package_referencers_for_asset(path)))
u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
opt=u.FbxImportUI();opt.automated_import_should_detect_type=False
opt.mesh_type_to_import=u.FBXImportType.FBXIT_SKELETAL_MESH
opt.import_as_skeletal=True;opt.import_mesh=True;opt.import_animations=False
opt.import_materials=False;opt.import_textures=False;opt.create_physics_asset=False
opt.skeleton=skeleton
opt.skeletal_mesh_import_data.set_editor_property('import_morph_targets',True)
opt.skeletal_mesh_import_data.set_editor_property('normal_import_method',u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
opt.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose',False)
task=u.AssetImportTask()
task.filename=str(Path(u.Paths.project_dir()).resolve().parents[1]/'Temporary/Workspace/work/Erling/Character/UnrealImport/SourceAssets/Meshes/SK_shirt.fbx')
task.destination_path='/Game/Erling/Meshes';task.destination_name='SK_shirt'
task.automated=True;task.replace_existing=True;task.save=False
task.options=opt;task.factory=u.FbxFactory()
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
shirt=u.load_asset(path)
assert shirt.skeleton==skeleton,'Skeleton changed'
after={m.get_name() for m in shirt.get_editor_property('morph_targets')}
assert before==after, 'Corrective names changed: '+str((len(before),len(after),before-after,after-before))
shirt.set_editor_property('materials',slots)
A.save_loaded_asset(shirt)
u.log('SHIRT_REIMPORT_VALIDATED corrective_count=%d skeleton=%s'%(len(after),skeleton.get_path_name()))
