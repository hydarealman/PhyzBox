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
	check(not _contains_type(game, "CanvasLayer"), "historical view contains no screen UI layer")
	check(game.get_node_or_null("HipparcosJ2000Sky") != null, "real catalogue star field is instantiated")
	var stars := game.get_node_or_null("HipparcosJ2000Sky") as MultiMeshInstance3D
	check(stars != null and stars.multimesh.instance_count == 83_337, "GPU sky contains the full bright-star selection")
	var earth := game.get_node_or_null("HistoricalBody_399") as Node3D
	var earth_surface := earth.get_node_or_null("Surface") as MeshInstance3D
	check(earth.visible and earth_surface.scale.x > 5.0, "Earth dominates the launch view at physical angular scale")
	check(game.get_node_or_null("VoyagerOnePhysicalModel") != null, "Voyager is a constructed spacecraft model")

	var initial_epoch: float = float(game.current_epoch)
	game.rate_index = 0
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
