extends Node3D

const HistoricalEphemeris = preload("res://scripts/historical_ephemeris.gd")
const PLANET_SHADER = preload("res://shaders/planet_surface.gdshader")
const STAR_SHADER = preload("res://shaders/hipparcos_stars.gdshader")
const GALACTIC_SKY_SHADER = preload("res://shaders/galactic_sky.gdshader")
const ATMOSPHERE_SHADER = preload("res://shaders/atmosphere.gdshader")
const SATURN_RING_SHADER = preload("res://shaders/saturn_rings.gdshader")
const EPHEMERIS_PATH := "res://data/voyager_ephemeris.phyz"
const STAR_CATALOG_PATH := "res://data/hipparcos_bright.phyzstars"
const AU_KM := 149_597_870.7
const VOYAGER_1 := -31
const SUN := 10
const EARTH := 399

const TIME_RATES := [60.0, 3600.0, 21_600.0, 86_400.0, 604_800.0, 2_592_000.0]
const TIME_RATE_LABELS := ["1分钟/秒", "1小时/秒", "6小时/秒", "1天/秒", "1周/秒", "30天/秒"]
const BODY_ORDER := [10, 199, 299, 399, 301, 499, 5, 6, 7, 8, 9]
const FOCUS_ORDER := [399, 5, 6, 7, 8, 6, 499, 299, 199, 301, 9, 10]
const BODY_DATA := {
	10: {"name": "太阳", "radius": 695_700.0, "color": Color("ffd27a")},
	199: {"name": "水星", "radius": 2_439.7, "color": Color("9a9188")},
	299: {"name": "金星", "radius": 6_051.8, "color": Color("d8aa65")},
	399: {"name": "地球", "radius": 6_371.0, "color": Color("4b91d1")},
	301: {"name": "月球", "radius": 1_737.4, "color": Color("b8b4a9")},
	499: {"name": "火星", "radius": 3_389.5, "color": Color("bd6245")},
	5: {"name": "木星", "radius": 69_911.0, "color": Color("caa57f")},
	6: {"name": "土星", "radius": 58_232.0, "color": Color("d8c58c")},
	7: {"name": "天王星", "radius": 25_362.0, "color": Color("83d7df")},
	8: {"name": "海王星", "radius": 24_622.0, "color": Color("496ed1")},
	9: {"name": "冥王星", "radius": 1_188.3, "color": Color("a99583")},
}
const BODY_PALETTES := {
	199: {"low": Color("343331"), "mid": Color("77736d"), "high": Color("bbb4a9"), "ice": Color("d7d2c8"), "cloud": Color("d8d4cb")},
	299: {"low": Color("6f4a22"), "mid": Color("b67b35"), "high": Color("f0c875"), "ice": Color("fff0bf"), "cloud": Color("f4d58d")},
	399: {"low": Color("0b4f8a"), "mid": Color("3d7651"), "high": Color("a49362"), "ice": Color("edf7fb"), "cloud": Color("eaf4f7")},
	301: {"low": Color("2e2d2b"), "mid": Color("77736d"), "high": Color("aaa59b"), "ice": Color("cbc7bf"), "cloud": Color("cbc7bf")},
	499: {"low": Color("3c1712"), "mid": Color("983b27"), "high": Color("d97848"), "ice": Color("ead8c9"), "cloud": Color("d9a17d")},
	5: {"low": Color("4b2c24"), "mid": Color("ad7650"), "high": Color("efd2a2"), "ice": Color("fff0d0"), "cloud": Color("f2d9b3")},
	6: {"low": Color("655037"), "mid": Color("c5a666"), "high": Color("f0d995"), "ice": Color("fff0c4"), "cloud": Color("f2dda8")},
	7: {"low": Color("174d59"), "mid": Color("59b6be"), "high": Color("b5e8e9"), "ice": Color("d7f5f4"), "cloud": Color("bdebed")},
	8: {"low": Color("07143c"), "mid": Color("1e459a"), "high": Color("4e7ed8"), "ice": Color("a7c7ff"), "cloud": Color("729ce8")},
	9: {"low": Color("392b27"), "mid": Color("8f705e"), "high": Color("c9ad91"), "ice": Color("ded0bf"), "cloud": Color("d3c2ac")},
}
const EVENT_SPECS := [
	{"utc": "1977-09-05T13:59:25", "title": "旅行者1号离开地球", "target": 399},
	{"utc": "1979-03-05T12:00:00", "title": "木星最近接", "target": 5},
	{"utc": "1980-11-12T23:46:00", "title": "土星最近接", "target": 6},
	{"utc": "1980-12-20T16:45:19", "title": "驶向星际空间", "target": 6},
	{"utc": "2012-08-25T00:00:00", "title": "进入星际空间", "target": 10},
]

var ephemeris
var current_epoch := 0.0
var playing := false
var rate_index := 4
var auto_slow := true
var view_mode := "follow"
var focus_body_id := EARTH
var automatic_focus := true
var events: Array[Dictionary] = []
var body_nodes: Dictionary = {}
var body_labels: Dictionary = {}
var trajectory_mesh: MeshInstance3D
var voyager_visual: Node3D
var voyager_map_marker: MeshInstance3D
var camera: Camera3D
var sun_light: DirectionalLight3D
var camera_fill_light: DirectionalLight3D
var map_grid: MeshInstance3D
var world_environment: Environment
var galactic_material: ShaderMaterial
var star_material: ShaderMaterial
var audience_grade := true
var camera_distance := 0.35
var camera_yaw := 0.0
var camera_pitch := 0.24
var camera_dragging := false
var last_window_title_second := -1

func _ready() -> void:
	ephemeris = HistoricalEphemeris.new()
	_build_world()
	if not ephemeris.load_file(EPHEMERIS_PATH):
		push_error(ephemeris.load_error)
		return
	_build_events()
	var launch_unix := Time.get_unix_time_from_datetime_string("1977-09-05T13:59:25")
	current_epoch = ephemeris.epoch_for_utc(launch_unix)
	_create_solar_system()
	_apply_color_grade()
	_build_reference_trajectory()
	_update_history_state()

func _build_world() -> void:
	var environment_node := WorldEnvironment.new()
	world_environment = Environment.new()
	galactic_material = ShaderMaterial.new()
	galactic_material.shader = GALACTIC_SKY_SHADER
	var sky := Sky.new()
	sky.sky_material = galactic_material
	world_environment.sky = sky
	world_environment.background_mode = Environment.BG_SKY
	world_environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	world_environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	world_environment.adjustment_enabled = true
	environment_node.environment = world_environment
	add_child(environment_node)

	camera = Camera3D.new()
	camera.name = "VoyagerCamera"
	camera.fov = 52.0
	camera.near = 0.001
	camera.far = 400.0
	add_child(camera)
	camera_fill_light = DirectionalLight3D.new()
	camera_fill_light.name = "SpacecraftVisibilityLight"
	camera_fill_light.light_color = Color("bfd9ef")
	camera_fill_light.light_cull_mask = 2
	camera_fill_light.shadow_enabled = false
	camera.add_child(camera_fill_light)

	sun_light = DirectionalLight3D.new()
	sun_light.light_color = Color("fff3d6")
	sun_light.light_energy = 1.72
	sun_light.shadow_enabled = false
	add_child(sun_light)

	map_grid = _create_map_grid()
	add_child(map_grid)
	_build_starfield()

	trajectory_mesh = MeshInstance3D.new()
	trajectory_mesh.name = "VoyagerReferenceTrajectory"
	add_child(trajectory_mesh)

func _build_starfield() -> void:
	var file := FileAccess.open(STAR_CATALOG_PATH, FileAccess.READ)
	if not file:
		push_error("无法打开 Hipparcos 星表：%s" % STAR_CATALOG_PATH)
		return
	var magic := file.get_buffer(9).get_string_from_ascii()
	if magic != "PHYZSTAR1":
		push_error("Hipparcos 星表格式错误")
		return
	var count := file.get_32()
	var multimesh := MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.use_colors = true
	multimesh.use_custom_data = true
	multimesh.instance_count = count
	var quad := QuadMesh.new()
	quad.size = Vector2.ONE
	multimesh.mesh = quad
	for index in range(count):
		var right_ascension := deg_to_rad(file.get_float())
		var declination := deg_to_rad(file.get_float())
		var magnitude := file.get_float()
		var color_index := file.get_float()
		var cos_dec := cos(declination)
		var direction_j2000 := Vector3(
			cos_dec * cos(right_ascension),
			cos_dec * sin(right_ascension),
			sin(declination)
		)
		var direction := Vector3(direction_j2000.x, direction_j2000.z, -direction_j2000.y)
		multimesh.set_instance_transform(index, Transform3D(Basis.IDENTITY, direction * 260.0))
		multimesh.set_instance_color(index, _star_color(color_index))
		multimesh.set_instance_custom_data(index, Color(_star_size(magnitude), _star_intensity(magnitude), 0.0, 1.0))
	var stars := MultiMeshInstance3D.new()
	stars.name = "HipparcosJ2000Sky"
	stars.multimesh = multimesh
	star_material = ShaderMaterial.new()
	star_material.shader = STAR_SHADER
	stars.material_override = star_material
	stars.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(stars)

func _star_size(magnitude: float) -> float:
	return clampf(0.23 + pow(10.0, -0.16 * magnitude) * 0.72, 0.24, 1.32)

func _star_intensity(magnitude: float) -> float:
	return clampf(pow(10.0, -0.18 * (magnitude - 2.0)), 0.22, 3.4)

func _star_color(color_index: float) -> Color:
	var temperature := 4600.0 * (1.0 / (0.92 * color_index + 1.7) + 1.0 / (0.92 * color_index + 0.62))
	temperature = clampf(temperature, 2400.0, 40_000.0) / 100.0
	var red: float
	var green: float
	var blue: float
	if temperature <= 66.0:
		red = 1.0
		green = clampf(0.39008158 * log(temperature) - 0.63184144, 0.0, 1.0)
	else:
		red = clampf(1.2929362 * pow(temperature - 60.0, -0.13320476), 0.0, 1.0)
		green = clampf(1.1298909 * pow(temperature - 60.0, -0.07551485), 0.0, 1.0)
	if temperature >= 66.0:
		blue = 1.0
	elif temperature <= 19.0:
		blue = 0.0
	else:
		blue = clampf(0.5432068 * log(temperature - 10.0) - 1.1962541, 0.0, 1.0)
	return Color(red, green, blue, 1.0)

func _apply_color_grade() -> void:
	if not world_environment:
		return
	if audience_grade:
		world_environment.background_energy_multiplier = 1.08
		world_environment.ambient_light_color = Color("6f8295")
		world_environment.ambient_light_energy = 0.58
		world_environment.tonemap_exposure = 1.48
		world_environment.tonemap_white = 4.6
		world_environment.adjustment_brightness = 1.12
		world_environment.adjustment_contrast = 1.055
		world_environment.adjustment_saturation = 1.10
		sun_light.light_energy = 2.05
		camera_fill_light.light_energy = 0.78
		galactic_material.set_shader_parameter("display_gain", 2.15)
		star_material.set_shader_parameter("display_gain", 1.75)
	else:
		world_environment.background_energy_multiplier = 0.70
		world_environment.ambient_light_color = Color("172a3b")
		world_environment.ambient_light_energy = 0.30
		world_environment.tonemap_exposure = 0.95
		world_environment.tonemap_white = 6.0
		world_environment.adjustment_brightness = 1.0
		world_environment.adjustment_contrast = 1.0
		world_environment.adjustment_saturation = 1.0
		sun_light.light_energy = 1.72
		camera_fill_light.light_energy = 0.16
		galactic_material.set_shader_parameter("display_gain", 0.92)
		star_material.set_shader_parameter("display_gain", 0.90)
	for body_id in BODY_ORDER:
		if not body_nodes.has(body_id) or body_id == SUN:
			continue
		var holder := body_nodes[body_id] as Node3D
		var surface := holder.get_node("Surface") as MeshInstance3D
		var surface_material := surface.material_override as ShaderMaterial
		var base_emission := 0.24 if body_id == EARTH else 0.125
		surface_material.set_shader_parameter("emission_strength", base_emission if audience_grade else base_emission * 0.24)
		surface_material.set_shader_parameter("rim_fill", 0.16 if audience_grade else 0.0)
		var atmosphere := holder.get_node_or_null("Atmosphere") as MeshInstance3D
		if atmosphere:
			var atmosphere_material := atmosphere.material_override as ShaderMaterial
			var base_intensity := _atmosphere_intensity(body_id)
			atmosphere_material.set_shader_parameter("intensity", base_intensity if audience_grade else base_intensity * 0.52)
	last_window_title_second = -1

func _toggle_color_grade() -> void:
	audience_grade = not audience_grade
	_apply_color_grade()
	_update_window_title()

func _create_map_grid() -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	instance.name = "SolarSystemScaleGrid"
	var mesh := ImmediateMesh.new()
	mesh.surface_begin(Mesh.PRIMITIVE_LINES)
	for radius in [1.0, 5.0, 10.0, 20.0, 30.0]:
		for segment in range(128):
			var a0 := TAU * float(segment) / 128.0
			var a1 := TAU * float(segment + 1) / 128.0
			mesh.surface_add_vertex(Vector3(cos(a0) * radius, 0.0, sin(a0) * radius))
			mesh.surface_add_vertex(Vector3(cos(a1) * radius, 0.0, sin(a1) * radius))
	mesh.surface_end()
	instance.mesh = mesh
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.albedo_color = Color(0.08, 0.38, 0.52, 0.18)
	instance.material_override = material
	return instance

func _build_events() -> void:
	events.clear()
	for spec in EVENT_SPECS:
		var event: Dictionary = spec.duplicate()
		var unix_time := Time.get_unix_time_from_datetime_string(str(spec.utc))
		event["epoch"] = ephemeris.epoch_for_utc(unix_time)
		events.append(event)

func _create_solar_system() -> void:
	for body_id in BODY_ORDER:
		var holder := Node3D.new()
		holder.name = "HistoricalBody_%s" % body_id
		add_child(holder)
		body_nodes[body_id] = holder
		var sphere := SphereMesh.new()
		sphere.radius = 1.0
		sphere.height = 2.0
		sphere.radial_segments = 48
		sphere.rings = 28
		var body := MeshInstance3D.new()
		body.name = "Surface"
		body.mesh = sphere
		body.material_override = _body_material(body_id)
		holder.add_child(body)
		if body_id in [299, 399, 499, 5, 6, 7, 8]:
			var atmosphere := MeshInstance3D.new()
			atmosphere.name = "Atmosphere"
			atmosphere.mesh = sphere
			var atmosphere_material := ShaderMaterial.new()
			atmosphere_material.shader = ATMOSPHERE_SHADER
			var color: Color = BODY_DATA[body_id].color
			atmosphere_material.set_shader_parameter("atmosphere_color", color.lightened(0.22))
			atmosphere_material.set_shader_parameter("intensity", _atmosphere_intensity(body_id))
			atmosphere.material_override = atmosphere_material
			holder.add_child(atmosphere)
		if body_id == 6:
			var rings := _create_saturn_rings()
			holder.add_child(rings)

		var label := Label3D.new()
		label.name = "MapLabel_%s" % body_id
		label.text = str(BODY_DATA[body_id].name).to_upper()
		label.font_size = 20
		label.outline_size = 5
		label.pixel_size = 0.012
		label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label.no_depth_test = true
		label.modulate = BODY_DATA[body_id].color.lightened(0.18)
		add_child(label)
		body_labels[body_id] = label

	voyager_visual = _create_voyager_model()
	voyager_visual.name = "VoyagerOnePhysicalModel"
	add_child(voyager_visual)
	voyager_map_marker = MeshInstance3D.new()
	voyager_map_marker.name = "VoyagerMapMarker"
	var marker_mesh := PrismMesh.new()
	marker_mesh.size = Vector3(0.025, 0.040, 0.025)
	voyager_map_marker.mesh = marker_mesh
	var marker_material := StandardMaterial3D.new()
	marker_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	marker_material.emission_enabled = true
	marker_material.emission = Color("63e6ed")
	marker_material.albedo_color = Color("63e6ed")
	voyager_map_marker.material_override = marker_material
	add_child(voyager_map_marker)

func _body_material(body_id: int) -> Material:
	var data: Dictionary = BODY_DATA[body_id]
	var color: Color = data.color
	if body_id == SUN:
		var sun_material := StandardMaterial3D.new()
		sun_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		sun_material.albedo_color = color
		sun_material.emission_enabled = true
		sun_material.emission = color * 2.4
		return sun_material
	var material := ShaderMaterial.new()
	material.shader = PLANET_SHADER
	var palette: Dictionary = BODY_PALETTES[body_id]
	material.set_shader_parameter("land_color", palette.mid)
	material.set_shader_parameter("highland_color", palette.high)
	material.set_shader_parameter("ocean_color", palette.low)
	material.set_shader_parameter("ice_color", palette.ice)
	material.set_shader_parameter("cloud_color", palette.cloud)
	material.set_shader_parameter("seed", absf(float(body_id)) * 0.0317)
	material.set_shader_parameter("gas_giant", 1.0 if body_id in [5, 6, 7, 8] else 0.0)
	material.set_shader_parameter("cloud_cover", 0.82 if body_id == EARTH else (0.94 if body_id == 299 else 0.06))
	material.set_shader_parameter("emission_strength", 0.14 if body_id == EARTH else 0.065)
	return material

func _create_saturn_rings() -> Node3D:
	var rings := Node3D.new()
	rings.name = "Rings"
	var mesh := ImmediateMesh.new()
	mesh.surface_begin(Mesh.PRIMITIVE_TRIANGLES)
	for segment in range(192):
		var angle0 := TAU * float(segment) / 192.0
		var angle1 := TAU * float(segment + 1) / 192.0
		var inner0 := Vector3(cos(angle0) * 1.12, 0.0, sin(angle0) * 1.12)
		var inner1 := Vector3(cos(angle1) * 1.12, 0.0, sin(angle1) * 1.12)
		var outer0 := Vector3(cos(angle0) * 2.36, 0.0, sin(angle0) * 2.36)
		var outer1 := Vector3(cos(angle1) * 2.36, 0.0, sin(angle1) * 2.36)
		mesh.surface_set_uv(Vector2(0.0, float(segment) / 192.0))
		mesh.surface_add_vertex(inner0)
		mesh.surface_set_uv(Vector2(1.0, float(segment) / 192.0))
		mesh.surface_add_vertex(outer0)
		mesh.surface_set_uv(Vector2(1.0, float(segment + 1) / 192.0))
		mesh.surface_add_vertex(outer1)
		mesh.surface_set_uv(Vector2(0.0, float(segment) / 192.0))
		mesh.surface_add_vertex(inner0)
		mesh.surface_set_uv(Vector2(1.0, float(segment + 1) / 192.0))
		mesh.surface_add_vertex(outer1)
		mesh.surface_set_uv(Vector2(0.0, float(segment + 1) / 192.0))
		mesh.surface_add_vertex(inner1)
	mesh.surface_end()
	var ring_surface := MeshInstance3D.new()
	ring_surface.mesh = mesh
	var material := ShaderMaterial.new()
	material.shader = SATURN_RING_SHADER
	ring_surface.material_override = material
	rings.add_child(ring_surface)
	return rings

func _atmosphere_intensity(body_id: int) -> float:
	match body_id:
		299: return 0.92
		399: return 1.18
		499: return 0.28
		5, 6: return 0.32
		7, 8: return 0.48
		_: return 0.0

func _create_voyager_model() -> Node3D:
	var root := Node3D.new()
	var metal := _material(Color("9aa3a8"), Color("25323a"), 0.08)
	var gold := _material(Color("b79a52"), Color("775d25"), 0.18)
	var dark := _material(Color("171d21"), Color("050708"), 0.02)
	var dish := _material(Color("d8d8cf"), Color("7a8284"), 0.08)
	_add_box(root, Vector3(0.0017, 0.0012, 0.0014), Vector3.ZERO, gold)
	var antenna := _add_sphere(root, 0.00185, Vector3(0.0, 0.00125, 0.0), dish)
	antenna.scale = Vector3(1.0, 0.18, 1.0)
	_add_cylinder(root, 0.00010, 0.00010, 0.0022, Vector3(0.0, 0.0020, 0.0), Vector3.ZERO, metal)
	_add_sphere(root, 0.00016, Vector3(0.0, 0.0031, 0.0), dark)
	_add_cylinder(root, 0.00010, 0.00010, 0.010, Vector3(-0.005, 0.0, 0.0), Vector3(0.0, 0.0, PI * 0.5), metal)
	for index in range(4):
		_add_cylinder(
			root, 0.00023, 0.00023, 0.00115,
			Vector3(-0.0068 - float(index) * 0.00078, 0.0, 0.0),
			Vector3(0.0, 0.0, PI * 0.5), dark
		)
	_add_cylinder(root, 0.000055, 0.000055, 0.012, Vector3(0.006, 0.0, 0.0), Vector3(0.0, 0.0, PI * 0.5), metal)
	_add_sphere(root, 0.00020, Vector3(0.012, 0.0, 0.0), dark)
	return root

func _build_reference_trajectory() -> void:
	var points: PackedVector3Array = ephemeris.track(VOYAGER_1, SUN, ephemeris.start_epoch(), ephemeris.end_epoch(), 2200)
	var mesh := ImmediateMesh.new()
	mesh.surface_begin(Mesh.PRIMITIVE_LINE_STRIP)
	for point in points:
		mesh.surface_add_vertex(point)
	mesh.surface_end()
	trajectory_mesh.mesh = mesh
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.albedo_color = Color(0.35, 0.92, 0.96, 0.55)
	material.emission_enabled = true
	material.emission = Color(0.20, 0.80, 0.86)
	trajectory_mesh.material_override = material

func _process(delta: float) -> void:
	if not ephemeris.loaded:
		return
	if playing:
		var effective_rate: float = TIME_RATES[rate_index]
		if auto_slow:
			effective_rate = minf(effective_rate, _automatic_rate_limit())
		current_epoch = minf(ephemeris.end_epoch(), current_epoch + delta * effective_rate)
		if current_epoch >= ephemeris.end_epoch():
			playing = false
	_update_history_state()

func _update_history_state() -> void:
	if automatic_focus:
		focus_body_id = _nearest_body()
	_update_body_transforms()
	_update_camera()
	_position_follow_craft()
	_update_window_title()

func _update_body_transforms() -> void:
	var map_mode := view_mode == "map"
	var focus_raw: Dictionary = ephemeris.relative_state(focus_body_id, VOYAGER_1, current_epoch, 1.0)
	var focus_distance := Vector3(focus_raw.get("position", Vector3(0.0, 0.0, -1.0))).length()
	var follow_scale := 12.0 / maxf(focus_distance, 1.0)
	map_grid.visible = map_mode
	trajectory_mesh.visible = map_mode
	voyager_map_marker.visible = map_mode
	voyager_visual.visible = not map_mode
	for body_id in BODY_ORDER:
		if not map_mode and body_id != focus_body_id:
			body_nodes[body_id].visible = false
			body_labels[body_id].visible = false
			continue
		var observer := SUN if map_mode else VOYAGER_1
		var scale_factor := 1.0 / AU_KM if map_mode else follow_scale
		var state: Dictionary = ephemeris.relative_state(body_id, observer, current_epoch, scale_factor)
		var holder: Node3D = body_nodes[body_id]
		var label: Label3D = body_labels[body_id]
		holder.visible = bool(state.valid)
		label.visible = map_mode and bool(state.valid)
		if not bool(state.valid):
			continue
		holder.position = state.position
		label.position = state.position + Vector3(0.0, 0.11, 0.0)
		var radius_km := float(BODY_DATA[body_id].radius)
		var visible_radius := _map_radius(body_id) if map_mode else radius_km * follow_scale
		var surface := holder.get_node("Surface") as MeshInstance3D
		surface.scale = Vector3.ONE * visible_radius
		var atmosphere := holder.get_node_or_null("Atmosphere") as MeshInstance3D
		if atmosphere:
			atmosphere.scale = Vector3.ONE * visible_radius * 1.035
		if body_id == 6:
			var rings := holder.get_node("Rings") as Node3D
			rings.scale = Vector3.ONE * visible_radius

	var voyager_state: Dictionary = ephemeris.relative_state(VOYAGER_1, SUN, current_epoch, 1.0 / AU_KM)
	if bool(voyager_state.valid):
		voyager_map_marker.position = voyager_state.position
		voyager_map_marker.rotation.y += 0.012
	var sun_state: Dictionary = ephemeris.relative_state(SUN, VOYAGER_1, current_epoch, 1.0)
	if bool(sun_state.valid):
		var sun_direction: Vector3 = Vector3(sun_state.position).normalized()
		if absf(sun_direction.dot(Vector3.UP)) < 0.98:
			sun_light.look_at(-sun_direction, Vector3.UP)

func _update_camera() -> void:
	if view_mode == "map":
		camera.near = 0.002
		camera.far = 600.0
		var horizontal := cos(camera_pitch) * camera_distance
		camera.position = Vector3(
			sin(camera_yaw) * horizontal,
			sin(camera_pitch) * camera_distance,
			cos(camera_yaw) * horizontal
		)
		camera.look_at(Vector3.ZERO, Vector3.UP)
		return
	camera.near = 0.001
	camera.far = 400.0
	var target_state: Dictionary = ephemeris.relative_state(focus_body_id, VOYAGER_1, current_epoch, 1.0)
	var direction := Vector3(0.0, 0.0, -1.0)
	if bool(target_state.valid) and Vector3(target_state.position).length_squared() > 1.0e-12:
		direction = Vector3(target_state.position).normalized()
	var up := Vector3.UP
	if absf(direction.dot(up)) > 0.94:
		up = Vector3.RIGHT
	var right := up.cross(direction).normalized()
	var back := (-direction).rotated(up, camera_yaw).normalized()
	var camera_up := up.rotated(right, camera_pitch)
	camera.position = back * camera_distance + camera_up * camera_distance * 0.28
	var focus_state: Dictionary = ephemeris.relative_state(focus_body_id, VOYAGER_1, current_epoch, 1.0)
	var focus_distance := maxf(Vector3(focus_state.get("position", Vector3.ONE)).length(), 1.0)
	var angular_radius := float(BODY_DATA[focus_body_id].radius) * 12.0 / focus_distance
	var cinematic_offset := right * clampf((angular_radius - 2.2) * 0.78, 0.0, 8.4)
	camera.look_at(direction * 12.0 + cinematic_offset, up)

func _position_follow_craft() -> void:
	if view_mode != "follow":
		voyager_visual.position = Vector3.ZERO
		return
	var screen_forward := -camera.global_basis.z
	var screen_right := camera.global_basis.x
	var screen_up := camera.global_basis.y
	voyager_visual.position = camera.position + screen_forward * 0.43 + screen_right * 0.095 - screen_up * 0.105
	var craft_velocity: Dictionary = ephemeris.relative_state(VOYAGER_1, SUN, current_epoch, 1.0)
	if bool(craft_velocity.valid):
		var forward := Vector3(craft_velocity.velocity).normalized()
		if not forward.is_zero_approx():
			var up := Vector3.UP if absf(forward.dot(Vector3.UP)) < 0.98 else Vector3.RIGHT
			voyager_visual.look_at(voyager_visual.position + forward, up)

func _nearest_body() -> int:
	var nearest := EARTH
	var nearest_surface_distance := INF
	for body_id in FOCUS_ORDER:
		var state: Dictionary = ephemeris.relative_state(body_id, VOYAGER_1, current_epoch, 1.0)
		if not bool(state.valid):
			continue
		var distance := maxf(0.0, Vector3(state.position).length() - float(BODY_DATA[body_id].radius))
		if distance < nearest_surface_distance:
			nearest_surface_distance = distance
			nearest = body_id
	return nearest

func _automatic_rate_limit() -> float:
	var limit: float = TIME_RATES[rate_index]
	for event in events:
		var distance := absf(float(event.epoch) - current_epoch)
		if distance < 6.0 * 3600.0:
			limit = minf(limit, 60.0)
		elif distance < 3.0 * 86_400.0:
			limit = minf(limit, 3600.0)
	return limit

func _toggle_playing() -> void:
	playing = not playing
	last_window_title_second = -1
	_update_window_title()

func _slower() -> void:
	rate_index = maxi(0, rate_index - 1)
	last_window_title_second = -1
	_update_window_title()

func _faster() -> void:
	rate_index = mini(TIME_RATES.size() - 1, rate_index + 1)
	last_window_title_second = -1
	_update_window_title()

func _seek_seconds(seconds: float) -> void:
	current_epoch = clampf(current_epoch + seconds, ephemeris.start_epoch(), ephemeris.end_epoch())
	playing = false
	_update_history_state()

func _toggle_auto_slow() -> void:
	auto_slow = not auto_slow
	last_window_title_second = -1
	_update_window_title()

func _toggle_view() -> void:
	view_mode = "map" if view_mode == "follow" else "follow"
	if view_mode == "map":
		camera_distance = 42.0
		camera_yaw = 0.32
		camera_pitch = 0.58
	else:
		camera_distance = 0.35
		camera_yaw = 0.0
		camera_pitch = 0.24
	_update_history_state()

func _event_selected(index: int) -> void:
	if index < 0 or index >= events.size():
		return
	var event: Dictionary = events[index]
	current_epoch = float(event.epoch)
	focus_body_id = int(event.target)
	automatic_focus = false
	playing = false
	_update_history_state()

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo:
		match event.physical_keycode:
			KEY_ESCAPE: get_tree().quit()
			KEY_SPACE: _toggle_playing()
			KEY_M: _toggle_view()
			KEY_A: _toggle_auto_slow()
			KEY_C: _toggle_color_grade()
			KEY_HOME: _event_selected(0)
			KEY_1: _event_selected(0)
			KEY_2: _event_selected(1)
			KEY_3: _event_selected(2)
			KEY_4: _event_selected(3)
			KEY_5: _event_selected(4)
			KEY_LEFT: _seek_seconds(-30.0 * 86_400.0 if event.shift_pressed else -86_400.0)
			KEY_RIGHT: _seek_seconds(30.0 * 86_400.0 if event.shift_pressed else 86_400.0)
			KEY_EQUAL, KEY_KP_ADD: _faster()
			KEY_MINUS, KEY_KP_SUBTRACT: _slower()
	elif event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT or event.button_index == MOUSE_BUTTON_RIGHT:
			camera_dragging = event.pressed
		elif event.pressed and event.button_index == MOUSE_BUTTON_WHEEL_UP:
			camera_distance *= 0.86
			camera_distance = clampf(camera_distance, 0.12 if view_mode == "follow" else 4.0, 1.40 if view_mode == "follow" else 120.0)
		elif event.pressed and event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			camera_distance *= 1.16
			camera_distance = clampf(camera_distance, 0.12 if view_mode == "follow" else 4.0, 1.40 if view_mode == "follow" else 120.0)
	elif event is InputEventMouseMotion and camera_dragging:
		camera_yaw -= event.relative.x * 0.006
		camera_pitch = clampf(camera_pitch + event.relative.y * 0.004, -0.8, 0.8)

func _map_radius(body_id: int) -> float:
	if body_id == SUN:
		return 0.16
	if body_id in [5, 6]:
		return 0.085
	if body_id in [7, 8]:
		return 0.065
	return 0.036

func _update_window_title() -> void:
	var unix_time: float = ephemeris.unix_time_for_epoch(current_epoch)
	var whole_second := floori(unix_time)
	if whole_second == last_window_title_second:
		return
	last_window_title_second = whole_second
	var date := Time.get_datetime_dict_from_unix_time(whole_second)
	var state := "播放" if playing else "暂停"
	var grade := "任务可视化" if audience_grade else "物理曝光"
	DisplayServer.window_set_title("PhyzBox · Voyager 1 · %04d-%02d-%02d %02d:%02d UTC · %s · %s · %s" % [
		date.year, date.month, date.day, date.hour, date.minute, state, TIME_RATE_LABELS[rate_index], grade
	])

func _material(albedo: Color, emission: Color, strength: float) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = albedo
	material.metallic = 0.62
	material.roughness = 0.38
	if strength > 0.0:
		material.emission_enabled = true
		material.emission = emission * strength
	return material

func _add_box(parent: Node3D, size: Vector3, position: Vector3, material: Material) -> MeshInstance3D:
	var mesh := BoxMesh.new()
	mesh.size = size
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.layers = 2
	instance.position = position
	instance.material_override = material
	parent.add_child(instance)
	return instance

func _add_cylinder(parent: Node3D, top_radius: float, bottom_radius: float, height: float, position: Vector3, rotation: Vector3, material: Material) -> MeshInstance3D:
	var mesh := CylinderMesh.new()
	mesh.top_radius = top_radius
	mesh.bottom_radius = bottom_radius
	mesh.height = height
	mesh.radial_segments = 20
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.layers = 2
	instance.position = position
	instance.rotation = rotation
	instance.material_override = material
	parent.add_child(instance)
	return instance

func _add_sphere(parent: Node3D, radius: float, position: Vector3, material: Material) -> MeshInstance3D:
	var mesh := SphereMesh.new()
	mesh.radius = radius
	mesh.height = radius * 2.0
	mesh.radial_segments = 28
	mesh.rings = 16
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.layers = 2
	instance.position = position
	instance.material_override = material
	parent.add_child(instance)
	return instance
