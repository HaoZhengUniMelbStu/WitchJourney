"""Build the Hex Sokoban cartoon levels (safe to re-run; it rebuilds everything it owns).

  Tools > Execute Python Script... > this file, or run it through the MCP.

Creates / updates:
  /Game/HexSokoban/Blueprints/Board/BP_HexTile, BP_HexGoalTile, BP_HexBox
                                         Blueprint children you customise (mesh, materials, components);
                                         created once, never overwritten
  /HexSokoban/Materials/M_HexToon        lit toon material (Color, Fill, TopBoost, Flash)
  /HexSokoban/Materials/MI_HexBackdrop   dark-blue backdrop instance
  /Game/HexSokoban/Maps/L_Hex_01..02     puzzle levels with lights, backdrop, post process
and points Project Settings > Hex Sokoban (this session) at the material and level list.
"""
import unreal

TAG = "HexLevel"
MAT_DIR = "/HexSokoban/Materials"
MAP_DIR = "/Game/HexSokoban/Maps"
BP_DIR = "/Game/HexSokoban/Blueprints/Board"

# Axial coords (q, r): +q = screen up, +r = screen up-right with the default board camera.
LEVELS = [
    {   # Push it up, walk round, push it up-left. Pushing straight up twice is a dead end (Z to undo).
        "name": "L_Hex_01",
        "player": (-1, 0),
        "tiles": [(-1, 0), (0, 0), (1, 0), (0, 1), (1, -1), (-1, 1), (1, 1), (2, 0)],
        "goals": [(2, -1)],
        "boxes": [(0, 0)],
    },
    {   # Two boxes: the order you walk matters.
        "name": "L_Hex_02",
        "player": (0, 0),
        "tiles": [(0, 0), (1, 0), (-1, 0), (-1, 1), (-1, 2), (0, 1), (0, 2), (1, -1)],
        "goals": [(2, 0), (-1, 3)],
        "boxes": [(1, 0), (-1, 1)],
    },
    {   # Ring: park one box on the near goal, walk round the ring, bring the far box down. (>= 7 pushes)
        "name": "L_Hex_03",
        "player": (0, 0),
        "tiles": [(0, 0), (-1, 1), (-1, 2), (0, 1), (0, 4), (-1, 4), (-1, 5), (1, 3), (0, 5), (2, 3), (2, 4), (1, 5)],
        "goals": [(0, 2), (0, 3)],
        "boxes": [(0, 1), (0, 4)],
    },
]

BACKDROP_SRGB = (22, 74, 150)

AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LEVEL_EDITOR = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def srgb(r, g, b, a=1.0):
    def lin(c):
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    return unreal.LinearColor(lin(r), lin(g), lin(b), a)


# ------------------------------------------------------------------ materials

def build_toon_graph(m):
    """Lit toon look + player-guidance parameters (Dim darkens, Error tints red)."""
    MEL.delete_all_material_expressions(m)

    def expr(cls, x, y):
        return MEL.create_material_expression(m, cls, x, y)

    def param(cls, name, default, x, y):
        e = expr(cls, x, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", default)
        return e

    link = MEL.connect_material_expressions
    color = param(unreal.MaterialExpressionVectorParameter, "Color", unreal.LinearColor(0.0, 0.3, 1.0, 1.0), -1100, -200)
    fill = param(unreal.MaterialExpressionScalarParameter, "Fill", 0.35, -900, 100)
    flash = param(unreal.MaterialExpressionScalarParameter, "Flash", 0.0, -900, 200)
    top = param(unreal.MaterialExpressionScalarParameter, "TopBoost", 0.25, -900, 300)
    dim = param(unreal.MaterialExpressionScalarParameter, "Dim", 0.0, -1100, -50)
    error = param(unreal.MaterialExpressionScalarParameter, "Error", 0.0, -1100, 30)
    red = expr(unreal.MaterialExpressionConstant3Vector, -1100, -330)
    red.set_editor_property("constant", unreal.LinearColor(1.0, 0.06, 0.04, 1.0))

    tinted = expr(unreal.MaterialExpressionLinearInterpolate, -850, -250)
    link(color, "", tinted, "A"); link(red, "", tinted, "B"); link(error, "", tinted, "Alpha")
    keep = expr(unreal.MaterialExpressionOneMinus, -850, -60)
    link(dim, "", keep, "")
    base = expr(unreal.MaterialExpressionMultiply, -650, -200)
    link(tinted, "", base, "A"); link(keep, "", base, "B")

    normal = expr(unreal.MaterialExpressionVertexNormalWS, -900, 420)
    mask = expr(unreal.MaterialExpressionComponentMask, -700, 420)
    for channel, on in (("r", False), ("g", False), ("b", True), ("a", False)):
        mask.set_editor_property(channel, on)
    up = expr(unreal.MaterialExpressionSaturate, -550, 420)
    top_mul = expr(unreal.MaterialExpressionMultiply, -400, 350)
    add_a = expr(unreal.MaterialExpressionAdd, -400, 150)
    add_b = expr(unreal.MaterialExpressionAdd, -250, 200)
    lit = expr(unreal.MaterialExpressionMultiply, -100, 0)
    glow_k = expr(unreal.MaterialExpressionConstant, -400, -380)
    glow_k.set_editor_property("r", 0.6)
    err_mul = expr(unreal.MaterialExpressionMultiply, -250, -380)
    err_glow = expr(unreal.MaterialExpressionMultiply, -100, -380)
    emissive = expr(unreal.MaterialExpressionAdd, 50, -100)
    rough = expr(unreal.MaterialExpressionConstant, -250, 500)
    rough.set_editor_property("r", 1.0)
    spec = expr(unreal.MaterialExpressionConstant, -250, 580)
    spec.set_editor_property("r", 0.0)

    link(normal, "", mask, ""); link(mask, "", up, "")
    link(up, "", top_mul, "A"); link(top, "", top_mul, "B")
    link(fill, "", add_a, "A"); link(flash, "", add_a, "B")
    link(add_a, "", add_b, "A"); link(top_mul, "", add_b, "B")
    link(base, "", lit, "A"); link(add_b, "", lit, "B")
    link(error, "", err_mul, "A"); link(glow_k, "", err_mul, "B")
    link(red, "", err_glow, "A"); link(err_mul, "", err_glow, "B")
    link(lit, "", emissive, "A"); link(err_glow, "", emissive, "B")

    out = MEL.connect_material_property
    prop = unreal.MaterialProperty
    out(base, "", prop.MP_BASE_COLOR)
    out(emissive, "", prop.MP_EMISSIVE_COLOR)
    out(rough, "", prop.MP_ROUGHNESS)
    out(spec, "", prop.MP_SPECULAR)
    MEL.recompile_material(m)


def build_toon_material():
    path = MAT_DIR + "/M_HexToon"
    if EAL.does_asset_exist(path):
        m = unreal.load_asset(path)
        if "Dim" not in [str(n) for n in MEL.get_scalar_parameter_names(m)]:
            build_toon_graph(m)   # older version without the guidance parameters
            EAL.save_asset(path)
        return m
    m = AT.create_asset("M_HexToon", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    build_toon_graph(m)
    EAL.save_asset(path)
    return m


def build_backdrop_material(parent):
    # Preserve the shared animated backdrop when rebuilding levels.
    if EAL.does_asset_exist('/HexSokoban/Materials/M_HexFlowBackdrop'):
        parent = unreal.load_asset('/HexSokoban/Materials/M_HexFlowBackdrop')
    path = MAT_DIR + "/MI_HexBackdrop"
    mi = unreal.load_asset(path) if EAL.does_asset_exist(path) else AT.create_asset(
        "MI_HexBackdrop", MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    MEL.set_material_instance_vector_parameter_value(mi, "Color", srgb(*BACKDROP_SRGB))
    MEL.set_material_instance_scalar_parameter_value(mi, "Fill", 0.45)
    MEL.set_material_instance_scalar_parameter_value(mi, "TopBoost", 0.0)
    EAL.save_asset(path)
    return mi


# ------------------------------------------------------------------ blueprints

def ensure_blueprint(name, parent):
    path = BP_DIR + "/" + name
    if not EAL.does_asset_exist(path):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        AT.create_asset(name, BP_DIR, unreal.Blueprint, factory)
        EAL.save_asset(path)
    return EAL.load_blueprint_class(path)


# ------------------------------------------------------------------ levels

def grid():
    s = unreal.get_default_object(unreal.HexSokobanSettings)
    o = s.get_editor_property("grid_origin")
    return s.get_editor_property("tile_spacing"), (o.x, o.y)


def to_world(coord, z, spacing, origin):
    q, r = coord
    return unreal.Vector(origin[0] + spacing * (q + 0.5 * r), origin[1] + spacing * 0.8660254 * r, z)


def spawn(cls, location, rotation=None, label=None):
    actor = ACTORS.spawn_actor_from_class(cls, location, rotation or unreal.Rotator(0, 0, 0))
    actor.set_editor_property("tags", [TAG])
    if label:
        actor.set_actor_label(label)
    return actor


def open_clean_level(path):
    if EAL.does_asset_exist(path):
        LEVEL_EDITOR.load_level(path)
        for actor in ACTORS.get_all_level_actors():
            if actor.actor_has_tag(TAG) or isinstance(actor, unreal.PlayerStart):
                ACTORS.destroy_actor(actor)
    else:
        LEVEL_EDITOR.new_level(path)


def build_environment(center, backdrop_mi):
    # Lighting: soft sun from the upper left so shadows fall down-right (like the reference).
    sun = spawn(unreal.DirectionalLight, unreal.Vector(center.x, center.y, 800),
                unreal.Rotator(roll=0.0, pitch=-58.0, yaw=-35.0), "Sun")
    sun_comp = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sun_comp.set_editor_property("intensity", 3.2)
    sun_comp.set_editor_property("light_color", unreal.Color(255, 246, 232, 255))
    sun_comp.set_editor_property("atmosphere_sun_light", True)

    spawn(unreal.SkyAtmosphere, unreal.Vector(center.x, center.y, 0), label="SkyAtmosphere")
    sky = spawn(unreal.SkyLight, unreal.Vector(center.x, center.y, 600), label="SkyLight")
    sky_comp = sky.get_component_by_class(unreal.SkyLightComponent)
    sky_comp.set_editor_property("real_time_capture", True)
    sky_comp.set_editor_property("intensity", 1.0)

    # Big flat backdrop just under the tiles.
    backdrop = spawn(unreal.StaticMeshActor, unreal.Vector(center.x, center.y, -1.0), label="Backdrop")
    mesh_comp = backdrop.static_mesh_component
    mesh_comp.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane.Plane"))
    mesh_comp.set_material(0, backdrop_mi)
    backdrop.set_actor_scale3d(unreal.Vector(400, 400, 1))

    # Fixed exposure so colours stay exactly as authored.
    ppv = spawn(unreal.PostProcessVolume, unreal.Vector(center.x, center.y, 0), label="PostProcess")
    ppv.set_editor_property("unbound", True)
    settings = ppv.get_editor_property("settings")
    for key, value in (("auto_exposure_min_brightness", 0.0), ("auto_exposure_max_brightness", 0.0),
                       ("vignette_intensity", 0.25), ("bloom_intensity", 0.2)):
        settings.set_editor_property("override_" + key, True)
        settings.set_editor_property(key, value)
    # Path Tracer "Shaded Visible Through Objects" fill (needs Custom Depth-Stencil = Enabled with Stencil).
    pt_pp = "/Game/HexSokoban/FX/PathTracer/Materials/MI_PT_PostProcess"
    if EAL.does_asset_exist(pt_pp):
        blend = unreal.WeightedBlendable()
        blend.set_editor_property("weight", 1.0)
        blend.set_editor_property("object", unreal.load_asset(pt_pp))
        blendables = settings.get_editor_property("weighted_blendables")
        blendables.set_editor_property("array", [blend])
        settings.set_editor_property("weighted_blendables", blendables)
    ppv.set_editor_property("settings", settings)


def build_level(spec, backdrop_mi, classes):
    path = MAP_DIR + "/" + spec["name"]
    open_clean_level(path)
    spacing, origin = grid()

    tiles = list(spec["tiles"])
    for goal in spec["goals"]:
        if goal in tiles:
            tiles.remove(goal)
    every = tiles + list(spec["goals"])
    xs = [to_world(c, 0, spacing, origin) for c in every]
    center = unreal.Vector(sum(v.x for v in xs) / len(xs), sum(v.y for v in xs) / len(xs), 0)

    build_environment(center, backdrop_mi)
    for coord in tiles:
        spawn(classes["tile"], to_world(coord, 0, spacing, origin), label="Tile_%d_%d" % coord)
    for coord in spec["goals"]:
        spawn(classes["goal"], to_world(coord, 0, spacing, origin), label="Goal_%d_%d" % coord)
    for coord in spec["boxes"]:
        spawn(classes["box"], to_world(coord, 80, spacing, origin), label="Box_%d_%d" % coord)
    spawn(unreal.PlayerStart, to_world(spec["player"], 160, spacing, origin), label="PlayerStart")

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", unreal.HexSokobanGameMode.static_class())
    LEVEL_EDITOR.save_current_level()
    return path


toon = build_toon_material()
backdrop = build_backdrop_material(toon)

settings_cdo = unreal.get_default_object(unreal.HexSokobanSettings)
settings_cdo.set_editor_property("default_color_material", toon)

classes = {
    "tile": ensure_blueprint("BP_HexTile", unreal.HexTile.static_class()),
    "goal": ensure_blueprint("BP_HexGoalTile", unreal.HexGoalTile.static_class()),
    "box": ensure_blueprint("BP_HexBox", unreal.HexBox.static_class()),
}
paths = [build_level(spec, backdrop, classes) for spec in LEVELS]
settings_cdo.set_editor_property("level_sequence", [unreal.load_asset(p) for p in paths])

LEVEL_EDITOR.load_level(paths[0])
print("Hex levels built: %s" % ", ".join(paths))
