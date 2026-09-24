import unreal

LEVEL_PATH = "/Game/Maps/L_TreasureSketchDemo"

unreal.EditorLevelLibrary.new_level(LEVEL_PATH)

player_start = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.PlayerStart, unreal.Vector(-2800.0, 0.0, 900.0)
)
player_start.set_actor_label("PlayerStart_Scout")

sun = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 2500.0), unreal.Rotator(-38.0, -28.0, 0.0)
)
sun.set_actor_label("Sun")
sun_component = sun.get_component_by_class(unreal.DirectionalLightComponent)
sun_component.set_editor_property("intensity", 7.0)

sky_light = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.SkyLight, unreal.Vector(0.0, 0.0, 1500.0)
)
sky_light.set_actor_label("SkyLight")
sky_component = sky_light.get_component_by_class(unreal.SkyLightComponent)
sky_component.set_editor_property("intensity", 1.0)

atmosphere = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.SkyAtmosphere, unreal.Vector(0.0, 0.0, 0.0)
)
atmosphere.set_actor_label("SkyAtmosphere")

height_fog = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.ExponentialHeightFog, unreal.Vector(0.0, 0.0, 0.0)
)
height_fog.set_actor_label("SeaMist")

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log(f"TREASURE_SKETCH_MAP_CREATED {LEVEL_PATH}")
