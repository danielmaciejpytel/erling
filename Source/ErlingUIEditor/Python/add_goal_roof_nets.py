"""Add only the missing visual roof nets; preserve all other scene edits."""
import unreal as u
e=u.get_editor_subsystem(u.EditorActorSubsystem)
l=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert l.load_level('/Game/Maps/Pitch_ArtDirection')
for actor in e.get_all_level_actors():
    if actor.get_actor_label()=='AD Roof net':e.destroy_actor(actor)
cube=u.load_asset('/Engine/BasicShapes/Cube')
material=u.load_asset('/Game/Erling/ArtDirection/M_Chalk')
assert cube and material
def bar(p,size):
    actor=e.spawn_actor_from_class(u.StaticMeshActor,u.Vector(*p))
    actor.set_actor_label('AD Roof net')
    actor.set_folder_path('Art Direction/Stadium')
    actor.static_mesh_component.set_static_mesh(cube)
    actor.static_mesh_component.set_material(0,material)
    actor.set_actor_scale3d(u.Vector(*(v/100 for v in size)))
    actor.set_actor_enable_collision(False)
    actor.static_mesh_component.set_collision_profile_name('NoCollision')
for s in [-1,1]:
    for y in range(-350,351,50):bar((s*2865,y,260),(250,2,2))
    for x in range(2740,2991,50):bar((s*x,0,260),(2,700,2))
nets=[a for a in e.get_all_level_actors() if a.get_actor_label()=='AD Roof net']
assert len(nets)==42 and all(not a.get_actor_enable_collision() for a in nets)
assert sum(a.get_actor_location().x>0 for a in nets)==21
assert l.save_current_level()
u.log('GOAL_ROOF_NETS_READY: 21 visual bars per goal; existing physics roofs preserved')
