extends SceneTree

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
	var telemetry_panel := game.find_child("TelemetryPanel", true, false)
	var procedural_backdrop := game.find_child("ProceduralBackdrop", true, false) as ColorRect
	var overlay := game.find_child("StoryOverlay", true, false)
	var action_button := game.find_child("StoryActionButton", true, false) as Button
	var guidance_button := game.find_child("GuidanceButton", true, false) as Button
	var commit_button := game.find_child("CommitButton", true, false) as Button
	var run_button := game.find_child("RunButton", true, false) as Button
	var impulse_x := game.find_child("ImpulseX", true, false) as LineEdit
	var simulation = game.get_node_or_null("PhysicsSimulation")

	check(panel != null, "mission cockpit exists")
	check(panel != null and panel.size.x <= 450.0 and panel.size.y <= 200.0,
		"mission card preserves the orbital view instead of becoming a text wall")
	check(dock != null, "compact maneuver dock exists")
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

	action_button.pressed.emit()
	await process_frame
	check(not overlay.visible, "briefing can be dismissed")
	guidance_button.pressed.emit()
	await process_frame
	check(impulse_x != null and abs(impulse_x.text.to_float()) > 0.01,
		"navigation guidance fills a usable maneuver")
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
