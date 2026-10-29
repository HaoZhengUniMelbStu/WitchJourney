"""Shared, texture-free flowing ink backdrop. Only changes the backdrop material instance."""
import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
path = '/HexSokoban/Materials/M_HexFlowBackdrop'
material = unreal.load_asset(path)
if not material:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_HexFlowBackdrop', '/HexSokoban/Materials', unreal.Material, unreal.MaterialFactoryNew())
MEL.delete_all_material_expressions(material)
material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property('two_sided', True)
pos = MEL.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -700, 0)
time = MEL.create_material_expression(material, unreal.MaterialExpressionTime, -700, 200)
flow = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, -350, 0)
inputs=[]
for name in ['Position', 'Clock']:
    item=unreal.CustomInput()
    item.set_editor_property('input_name', name)
    inputs.append(item)
flow.set_editor_property('inputs', inputs)
flow.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
flow.set_editor_property('description', 'Slow enchanted ink currents and fine contour threads')
flow.set_editor_property('code', '''
float2 p = Position.xy / 1800.0;
float t = Clock * 0.025;
float2 q = p + float2(sin(p.y*1.4+t), cos(p.x*1.2-t))*0.32;
float n = sin(q.x*1.1 + q.y*0.7 + t)*0.5
        + sin(q.y*1.8-q.x*0.6-t*0.7)*0.28
        + sin(q.x*3.1+q.y*2.4+t*0.4)*0.12;
float blend = smoothstep(-0.85,0.85,n);
float3 ink = lerp(float3(0.018,0.019,0.043), float3(0.053,0.037,0.085), blend);
float teal = pow(saturate(0.5+0.5*sin(q.y*0.8-q.x*1.3-t*0.6)),3);
ink = lerp(ink,float3(0.025,0.062,0.070),teal*0.32);
float phase = n*18.0 + p.y*2.0;
float contour = 1.0-smoothstep(0.025,0.065,abs(sin(phase)));
float veil = 0.5+0.5*sin(p.x*0.6+p.y*0.9+t);
ink += float3(0.023,0.027,0.035)*contour*veil*0.24;
return ink * 0.65;
''')
MEL.connect_material_expressions(pos, '', flow, 'Position')
MEL.connect_material_expressions(time, '', flow, 'Clock')
MEL.connect_material_property(flow, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.recompile_material(material)
EAL.save_loaded_asset(material)
instance=unreal.load_asset('/HexSokoban/Materials/MI_HexBackdrop')
MEL.set_material_instance_parent(instance, material)
MEL.update_material_instance(instance)
EAL.save_loaded_asset(instance)
print('Saved flowing backdrop and shared instance:', material.get_path_name())
