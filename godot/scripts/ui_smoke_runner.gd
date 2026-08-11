extends SceneTree

const Campaign = preload("res://scripts/campaign.gd")

var failures := 0

func _init() -> void:
	call_deferred("_run")

func check(condition: bool, message: String) -> void:
	if not condition:
		failures += 1
		printerr("FAIL: " + message)

func _run() -> void:
	var scene: PackedScene = load("res://scenes/main.tscn")
	var game := scene.instantiate()
	root.add_child(game)
	for frame in range(5):
		await process_frame

	var panel := game.find_child("MissionPanel", true, false)
	var dock := game.find_child("CommandDock", true, false)
	var flight_hud := game.find_child("FlightHUD", true, false)
	var flight_reticle := game.find_child("FlightReticle", true, false)
	var telemetry_panel := game.find_child("TelemetryPanel", true, false)
	var procedural_backdrop := game.find_child("ProceduralBackdrop", true, false) as ColorRect
	var overlay := game.find_child("StoryOverlay", true, false)
	var action_button := game.find_child("StoryActionButton", true, false) as Button
	var guidance_button := game.find_child("GuidanceButton", true, false) as Button
	var commit_button := game.find_child("CommitButton", true, false) as Button
	var run_button := game.find_child("RunButton", true, false) as Button
	var impulse_x := game.find_child("ImpulseX", true, false) as LineEdit
	var burn_time := game.find_child("BurnTime", true, false) as LineEdit
	var simulation = game.get_node_or_null("PhysicsSimulation")

	check(panel != null, "mission cockpit exists")
	check(panel != null and panel.size.x <= 450.0 and panel.size.y <= 200.0,
		"mission card preserves the orbital view instead of becoming a text wall")
	check(dock != null, "compact maneuver dock exists")
	check(dock != null and not dock.visible, "maneuver dock stays out of the direct-flight view")
	check(flight_hud != null and flight_hud.visible, "direct-flight HUD is the default playable interface")
	check(flight_reticle != null and flight_reticle.visible, "flight view has a central attitude reticle")
	check(telemetry_panel != null and not telemetry_panel.visible,
		"professional telemetry is available but collapsed by default")
	check(procedural_backdrop != null and procedural_backdrop.material is ShaderMaterial,
		"briefing uses a code-rendered procedural background")
	check(overlay != null and overlay.visible, "story briefing is visible on mission start")
	check(action_button != null, "briefing has an entry action")
	check(guidance_button != null and commit_button != null and run_button != null,
		"core playable controls exist")
	check(get_nodes_in_group("playable_control").size() >= 12,
		"cockpit exposes a complete control set")
	check(simulation != null, "physics adapter is attached to the game scene")
	check(Dictionary(game.get("body_nodes")).size() >= 3, "mission bodies have visual nodes")
	var player_id := int(simulation.get_mission_definition().player_body_id)
	var player_holder: Node3D = Dictionary(game.get("body_nodes")).get(player_id)
	var spacecraft := player_holder.get_node_or_null("SpacecraftVisual") if player_holder else null
	check(spacecraft != null and spacecraft.get_child_count() >= 12,
		"player is rendered as a component-built spacecraft rather than a sphere")
	check(get_nodes_in_group("player_drive_plume").size() == 4,
		"player spacecraft has four procedural drive plumes")
	check(game.find_child("ProceduralAtmosphere", true, false) != null,
		"mission includes a code-rendered procedural planet atmosphere")

	action_button.pressed.emit()
	await process_frame
	check(not overlay.visible, "briefing can be dismissed")
	check(str(game.get("view_mode")) == "flight" and bool(game.get("running")),
		"briefing hands control directly to the player")
	game.call("_set_view_mode", "map")
	await process_frame
	check(dock.visible and not flight_hud.visible,
		"map view exposes planning controls without covering flight")
	guidance_button.pressed.emit()
	await process_frame
	check(impulse_x != null and abs(impulse_x.text.to_float()) < 0.001,
		"guidance shows spatial information without injecting a puzzle answer")
	var hint: Vector3 = Campaign.mission(0).hint
	var world_hint := Vector3(hint.x, hint.z, -hint.y)
	var orbital_hint: Vector3 = game.call("_inertial_world_to_orbital", world_hint)
	game.call("_set_impulse", orbital_hint)
	burn_time.text = "%.6f" % (simulation.get_time() + 0.001)
	commit_button.pressed.emit()
	await process_frame
	check(bool(simulation.get_scheduled_impulse().active),
		"guided maneuver can be committed from the cockpit")
	var definition: Dictionary = simulation.get_mission_definition()
	while simulation.get_time() <= float(definition.deadline) + 0.01 and not bool(game.get("mission_finished")):
		simulation.advance(0.01)
		game.call("_update_telemetry")
	check(bool(game.get("mission_finished")), "guided first mission reaches a result")
	check(int(game.get("unlocked_mission")) >= 1, "mission completion unlocks the next story mission")
	check(overlay.visible, "mission completion opens a story debrief")

	if failures == 0:
		print("PhyzBox UI smoke tests passed")
		quit(0)
	else:
		printerr("%d UI smoke test(s) failed" % failures)
		quit(1)
