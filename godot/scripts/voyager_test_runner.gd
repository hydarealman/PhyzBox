extends SceneTree

const HistoricalEphemeris = preload("res://scripts/historical_ephemeris.gd")

var failures := 0

func _init() -> void:
	call_deferred("_run")

func check(condition: bool, message: String) -> void:
	if not condition:
		failures += 1
		printerr("FAIL: " + message)

func _contains_type(node: Node, type_name: String) -> bool:
	if node.get_class() == type_name:
		return true
	for child in node.get_children():
		if _contains_type(child, type_name):
			return true
	return false

func _run() -> void:
	var ephemeris := HistoricalEphemeris.new()
	check(ephemeris.load_file("res://data/voyager_ephemeris.phyz"), "official Voyager ephemeris loads")
	check(ephemeris.body_ids.has(-31), "Voyager 1 is present in ephemeris")
	check(ephemeris.body_ids.has(-32), "Voyager 2 is present in ephemeris")
	check(ephemeris.body_ids.has(399), "Earth is present in ephemeris")
	check(ephemeris.epochs.size() > 10_000, "historical ephemeris has long-duration samples")

	var launch_unix := Time.get_unix_time_from_datetime_string("1977-09-05T13:59:25")
	var launch_epoch: float = ephemeris.epoch_for_utc(launch_unix)
	var launch_earth: Dictionary = ephemeris.relative_state(-31, 399, launch_epoch, 1.0)
	check(bool(launch_earth.valid), "Voyager 1 launch state is valid")
	var launch_distance := Vector3(launch_earth.get("position", Vector3.ZERO)).length()
	check(launch_distance > 7_000.0 and launch_distance < 9_000.0,
		"official first state is close to Earth")

	var jupiter_unix := Time.get_unix_time_from_datetime_string("1979-03-05T12:00:00")
	var jupiter_epoch: float = ephemeris.epoch_for_utc(jupiter_unix)
	var jupiter_state: Dictionary = ephemeris.relative_state(-31, 5, jupiter_epoch, 1.0)
	var jupiter_distance := Vector3(jupiter_state.get("position", Vector3.ZERO)).length()
	check(bool(jupiter_state.valid) and jupiter_distance > 300_000.0 and jupiter_distance < 450_000.0,
		"Jupiter encounter distance matches the historical flyby neighborhood")

	var star_file := FileAccess.open("res://data/hipparcos_bright.phyzstars", FileAccess.READ)
	check(star_file != null, "Hipparcos runtime catalogue exists")
	if star_file:
		check(star_file.get_buffer(9).get_string_from_ascii() == "PHYZSTAR1", "star catalogue format is valid")
		check(star_file.get_32() == 83_337, "all selected Hipparcos stars are packaged")

	var packed: PackedScene = load("res://scenes/voyager_main.tscn")
	var game := packed.instantiate()
	root.add_child(game)
	for frame in range(3):
		await process_frame
	var time_layer := game.get_node_or_null("TimeControlLayer") as CanvasLayer
	var time_slider := game.get_node_or_null("TimeControlLayer/TimeControlPanel/TimeControlMargin/TimeControlRow/RateControls/RateSlider") as HSlider
	var time_rate_label := game.get_node_or_null("TimeControlLayer/TimeControlPanel/TimeControlMargin/TimeControlRow/RateControls/RateHeader/CurrentRate") as Label
	check(time_layer != null, "historical view provides a compact time-control layer")
	check(time_slider != null and int(time_slider.max_value) == game.TIME_RATES.size() - 1,
		"time slider exposes the complete rate range")
	check(time_rate_label != null and time_rate_label.text.begins_with(game.TIME_RATE_LABELS[game.rate_index]),
		"time control displays the current rate")
	check(game.get_node_or_null("HipparcosJ2000Sky") != null, "real catalogue star field is instantiated")
	var stars := game.get_node_or_null("HipparcosJ2000Sky") as MultiMeshInstance3D
	check(stars != null and stars.multimesh.instance_count == 83_337, "GPU sky contains the full bright-star selection")
	var earth := game.get_node_or_null("HistoricalBody_399") as Node3D
	var earth_surface := earth.get_node_or_null("Surface") as MeshInstance3D
	check(earth.visible and earth_surface.scale.x > 5.0, "Earth dominates the launch view at physical angular scale")
	check(game.get_node_or_null("VoyagerOnePhysicalModel") != null, "Voyager is a constructed spacecraft model")
	check(bool(game.audience_grade), "audience-friendly mission visualization is the default color grade")
	check(float(game.world_environment.tonemap_exposure) > 1.2, "default grade lifts exposure for viewers")
	check(Color(game.world_environment.ambient_light_color).get_luminance() > 0.25,
		"default grade keeps shadow detail visible")
	var mars_material := (game.get_node("HistoricalBody_499/Surface") as MeshInstance3D).material_override as ShaderMaterial
	var saturn_material := (game.get_node("HistoricalBody_6/Surface") as MeshInstance3D).material_override as ShaderMaterial
	var neptune_material := (game.get_node("HistoricalBody_8/Surface") as MeshInstance3D).material_override as ShaderMaterial
	var saturn_rings := game.get_node_or_null("HistoricalBody_6/Rings") as Node3D
	var mars_red: Color = mars_material.get_shader_parameter("land_color")
	var saturn_gold: Color = saturn_material.get_shader_parameter("highland_color")
	var neptune_blue: Color = neptune_material.get_shader_parameter("highland_color")
	check(mars_red.r > mars_red.g * 1.6, "Mars palette preserves iron-oxide red")
	check(saturn_gold.r > saturn_gold.b * 1.4 and saturn_gold.g > saturn_gold.b * 1.2,
		"Saturn palette preserves pale gold clouds")
	check(saturn_rings != null and saturn_rings.get_child_count() == 1,
		"Saturn uses the procedural thin-ring model")
	check(neptune_blue.b > neptune_blue.r * 1.8, "Neptune palette preserves deep blue appearance")
	var bright_exposure: float = float(game.world_environment.tonemap_exposure)
	game.call("_toggle_color_grade")
	check(not bool(game.audience_grade) and float(game.world_environment.tonemap_exposure) < bright_exposure,
		"C switches to lower physical exposure")
	game.call("_toggle_color_grade")
	time_slider.value = 2.0
	await process_frame
	check(int(game.rate_index) == 2 and absf(float(game.TIME_RATES[game.rate_index]) - 3600.0) < 0.01,
		"dragging the time slider selects one hour per second")
	check(not bool(game.auto_slow), "dragging the time slider enters manual-rate mode")
	check(time_rate_label != null and time_rate_label.text == "1小时/秒  ·  手动",
		"slider selection displays the effective manual rate")
	var manual_epoch: float = float(game.current_epoch)
	game.playing = true
	game.call("_process", 2.0)
	check(absf(float(game.current_epoch) - manual_epoch - 7200.0) < 0.01,
		"manual slider rate is not clamped by launch-event auto slowdown")
	game.playing = false
	game.call("_faster")
	check(int(game.rate_index) == 3 and int(time_slider.value) == 3 and not bool(game.auto_slow),
		"keyboard rate controls remain synchronized with the slider")
	game.call("_set_time_rate_index", game.TIME_RATES.size() - 1)
	game.auto_slow = true
	game.playing = true
	game.call("_update_time_controls")
	check(time_rate_label.text.contains("自动减速") and time_rate_label.text.contains("1分钟/秒"),
		"close encounters display the effective automatic-slowdown rate")

	var initial_epoch: float = float(game.current_epoch)
	game.call("_set_time_rate_index", 0)
	game.auto_slow = false
	game.playing = true
	game.call("_process", 2.0)
	check(absf(float(game.current_epoch) - initial_epoch - 120.0) < 0.01, "time-rate control advances historical time")
	game.playing = false
	game.call("_toggle_view")
	check(str(game.view_mode) == "map", "M view switch reaches the solar-system map")
	check(bool(game.get_node("SolarSystemScaleGrid").visible), "map orbit reference is visible")
	game.call("_event_selected", 1)
	check(int(game.focus_body_id) == 5, "Jupiter history shortcut selects the correct body")
	check(not bool(game.playing), "history jumps pause playback")

	game.queue_free()
	if failures == 0:
		print("PhyzBox Voyager historical-playback tests passed")
		quit(0)
	else:
		printerr("%d Voyager test(s) failed" % failures)
		quit(1)
