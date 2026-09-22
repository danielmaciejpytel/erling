"""Second visual pass: preserve texture inputs, wardrobe and authored scene layout."""
import unreal as u

A = u.EditorAssetLibrary
M = u.MaterialEditingLibrary
ROOT = '/Game/Erling/ArtDirection/'

def node(mat, cls, **props):
    result = M.create_material_expression(mat, cls)
    for key, value in props.items():
        result.set_editor_property(key, value)
    return result

def custom(mat, names, code):
    result = node(mat, u.MaterialExpressionCustom,
                  code=code, output_type=u.CustomMaterialOutputType.CMOT_FLOAT3,
                  desc='Painterly surface pass')
    inputs = []
    for name in names:
        value = u.CustomInput()
        value.set_editor_property('input_name', name)
        inputs.append(value)
    result.set_editor_property('inputs', inputs)
    return result

for name in ['body', 'hair', 'shirt', 'pants', 'shoe', 'Ball']:
    mat = u.load_asset(ROOT + 'M_' + name)
    assert mat
    base = M.get_material_property_input_node(mat, u.MaterialProperty.MP_BASE_COLOR)
    if base.get_editor_property('desc') == 'Painterly surface pass':
        continue
    # UVs move with the animated mesh; world-space noise would slide over the skin.
    uv = node(mat, u.MaterialExpressionTextureCoordinate)
    normal = node(mat, u.MaterialExpressionPixelNormalWS)
    view = node(mat, u.MaterialExpressionCameraVectorWS)
    strength = .035 if name == 'body' else .06
    ink = .23 if name == 'body' else .34
    effect = custom(mat, ['Base', 'UV', 'N', 'V'], '''
float2 q=UV*float2(38,53);
float2 cell=floor(q); float2 f=frac(q); f=f*f*(3-2*f);
float a=frac(sin(dot(cell,float2(127.1,311.7)))*43758.5453);
float b=frac(sin(dot(cell+float2(1,0),float2(127.1,311.7)))*43758.5453);
float c=frac(sin(dot(cell+float2(0,1),float2(127.1,311.7)))*43758.5453);
float d=frac(sin(dot(cell+1,float2(127.1,311.7)))*43758.5453);
float brush=lerp(lerp(a,b,f.x),lerp(c,d,f.x),f.y)-.5;
float fade=1-saturate(max(length(ddx(q)),length(ddy(q))));
float facing=abs(dot(normalize(N),normalize(V)));
float edge=1-smoothstep(.02,.24,facing);
float3 painted=Base*(1+STRENGTH*brush*fade);
return lerp(painted,painted*float3(.30,.34,.44),edge*INK);
'''.replace('STRENGTH', str(strength)).replace('INK', str(ink)))
    for source, dest in [(base,'Base'),(uv,'UV'),(normal,'N'),(view,'V')]:
        M.connect_material_expressions(source, '', effect, dest)
    M.connect_material_property(effect, '', u.MaterialProperty.MP_BASE_COLOR)
    M.recompile_material(mat)
    A.save_loaded_asset(mat)

# Replace the high-frequency sine pattern by derivative-filtered, irregular patches.
for name in ['GrassA', 'GrassB', 'Outside', 'Stadium', 'Concrete', 'Seats']:
    mat = u.load_asset(ROOT + 'M_' + name)
    base = M.get_material_property_input_node(mat, u.MaterialProperty.MP_BASE_COLOR)
    if base.get_editor_property('desc') == 'Painterly surface pass':
        continue
    grass = name in ['GrassA', 'GrassB', 'Outside']
    if grass:
        # Existing grass Custom expression retains its original palette input.
        assert isinstance(base, u.MaterialExpressionCustom)
        effect = base
        effect.set_editor_property('desc', 'Painterly surface pass')
    else:
        effect = custom(mat, ['P','Tint'], '')
        pos = node(mat, u.MaterialExpressionWorldPosition)
        M.connect_material_expressions(pos, '', effect, 'P')
        M.connect_material_expressions(base, '', effect, 'Tint')
        M.connect_material_property(effect, '', u.MaterialProperty.MP_BASE_COLOR)
    effect.set_editor_property('code', '''
float3 p=P*.024;
float broad=sin(p.x+sin(p.y*.71))*sin(p.y*.67+p.z*.41);
float2 q=COORD;
float2 cell=floor(q);float2 f=frac(q);f=f*f*(3-2*f);
float a=frac(sin(dot(cell,float2(127.1,311.7)))*43758.5453);
float b=frac(sin(dot(cell+float2(1,0),float2(127.1,311.7)))*43758.5453);
float c=frac(sin(dot(cell+float2(0,1),float2(127.1,311.7)))*43758.5453);
float d=frac(sin(dot(cell+1,float2(127.1,311.7)))*43758.5453);
float grain=lerp(lerp(a,b,f.x),lerp(c,d,f.x),f.y)-.5;
float fade=1-saturate(max(length(ddx(q)),length(ddy(q))));
return Tint*(1+BROAD*broad+GRAIN*grain*fade);
'''.replace('COORD', 'P.xy*float2(.12,.035)' if grass else '(P.xy+P.zz)*.035')
        .replace('BROAD', '.055' if grass else '.025')
        .replace('GRAIN', '.17' if grass else '.055'))
    M.recompile_material(mat)
    A.save_loaded_asset(mat)
u.log('PAINTERLY PASS COMPLETE: 12 materials; scene and gameplay unchanged')
