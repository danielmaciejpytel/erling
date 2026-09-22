"""Read-only validation of the art map and the preserved gameplay geometry."""
import unreal as u

editor=u.get_editor_subsystem(u.EditorActorSubsystem)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)

def collision_snapshot(path):
    assert levels.load_level(path)
    result=[]
    for actor in editor.get_all_level_actors():
        if isinstance(actor,u.StaticMeshActor) and actor.get_actor_enable_collision():
            if actor.get_actor_label()=='Stand':continue
            p=actor.get_actor_location();r=actor.get_actor_rotation();s=actor.get_actor_scale3d()
            result.append((actor.get_actor_label(),tuple(round(v,4) for v in [p.x,p.y,p.z,r.pitch,r.yaw,r.roll,s.x,s.y,s.z]),actor.static_mesh_component.static_mesh.get_path_name()))
    return sorted(result)

original=collision_snapshot('/Game/Maps/Pitch')
styled=collision_snapshot('/Game/Maps/Pitch_ArtDirection')
assert original==styled, 'Gameplay collision geometry changed: removed=%s added=%s' % (set(original)-set(styled),set(styled)-set(original))
actors=editor.get_all_level_actors()
assert sum('PitchBuilt' in [str(t) for t in a.tags] for a in actors)==1
assert sum(a.get_actor_label()=='AD Sky' for a in actors)==1
assert sum(isinstance(a,u.PostProcessVolume) and a.get_actor_label().startswith('AD ') for a in actors)==1
roofs=[a for a in actors if a.get_actor_label()=='AD Roof net']
assert len(roofs)==42 and sum(a.get_actor_location().x>0 for a in roofs)==21
assert all(abs(a.get_actor_location().z-260)<.01 for a in roofs)
for actor in actors:
    if isinstance(actor,u.StaticMeshActor) and actor.get_actor_label().startswith('AD '):
        assert not actor.get_actor_enable_collision(),actor.get_actor_label()
        assert actor.static_mesh_component.get_material(0),actor.get_actor_label()
for name in ['body','hair','shirt','pants','shoe','Face','Ball','Sky','GrassA','GrassB']:
    assert u.load_asset('/Game/Erling/ArtDirection/M_'+name),name
u.log('ART_DIRECTION_VALIDATED: original collision preserved; one sky and postprocess; decor non-colliding; required materials present')
