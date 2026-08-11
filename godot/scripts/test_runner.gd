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
	var simulation = PhyzSimulation.new()
	root.add_child(simulation)

	for mission_index in range(5):
		simulation.load_mission(mission_index)
		var definition: Dictionary = simulation.get_mission_definition()
		var bodies: Array = simulation.get_bodies()
		check(int(definition.index) == mission_index, "mission index roundtrip")
		check(not str(definition.name).is_empty(), "mission has a name")
		check(not str(definition.objective).is_empty(), "mission has an objective")
		check(bodies.size() >= 3, "mission has a physical system")
		check(float(definition.delta_v_budget) > 0.0, "mission has a delta-v budget")
		var prediction: Array = simulation.predict(
			int(definition.player_body_id), 0.0, 0.01, 0.0, simulation.get_time() + 0.02, 0.05, 0.005)
		check(prediction.size() == 11, "mission prediction has deterministic samples")
		var report: Dictionary = simulation.advance(0.001)
		check(int(report.status) == 0, "mission advances successfully")
		check(simulation.get_time() > 0.0, "mission time advances")

	check(Campaign.CHAPTERS.size() == 3, "campaign contains three chapters")
	check(Campaign.MISSIONS.size() == 5, "campaign maps all five physical missions")
	for mission_index in range(Campaign.MISSIONS.size()):
		var mission: Dictionary = Campaign.mission(mission_index)
		check(not str(mission.story).is_empty(), "mission has story text")
		check(not str(mission.tutorial).is_empty(), "mission has tutorial guidance")
		simulation.load_mission(mission_index)
		var mission_definition: Dictionary = simulation.get_mission_definition()
		var hint: Vector3 = mission.hint
		if not hint.is_zero_approx():
			check(simulation.apply_impulse(
				int(mission_definition.player_body_id), hint.x, hint.y, hint.z),
				"tested navigation hint fits the mission budget")
		var solved := false
		while simulation.get_time() <= float(mission_definition.deadline) + 0.06:
			var mission_evaluation: Dictionary = simulation.evaluate_mission()
			if bool(mission_evaluation.success):
				solved = true
				break
			if bool(mission_evaluation.failed):
				break
			simulation.advance(0.01)
		check(solved, "navigation hint provides a playable solution for mission %d" % mission_index)

	simulation.load_mission(0)
	var definition: Dictionary = simulation.get_mission_definition()
	var player_id := int(definition.player_body_id)
	var velocity_before := Vector3.ZERO
	for body in simulation.get_bodies():
		if int(body.id) == player_id:
			velocity_before = Vector3(float(body.velocity[0]), float(body.velocity[1]), float(body.velocity[2]))
	var controlled_report: Dictionary = simulation.advance_controlled(0.01, 1.0, 0.0, 0.0, 0.5)
	var velocity_after := Vector3.ZERO
	for body in simulation.get_bodies():
		if int(body.id) == player_id:
			velocity_after = Vector3(float(body.velocity[0]), float(body.velocity[1]), float(body.velocity[2]))
	check(int(controlled_report.status) == 0, "finite-thrust propagation advances successfully")
	check(float(controlled_report.thrust_delta_v) > 0.024,
		"finite thrust reports its integrated delta-v")
	check(float(simulation.get_mission_definition().delta_v_spent) > 0.024,
		"finite thrust consumes the shared mission fuel budget")
	check((velocity_after - velocity_before).length() > 0.02,
		"finite thrust changes the authoritative libphyz state")

	simulation.load_mission(0)
	definition = simulation.get_mission_definition()
	check(simulation.apply_impulse(int(definition.player_body_id), 0.0, 0.1, 0.0),
		"valid impulse is accepted")
	check(not simulation.apply_impulse(int(definition.player_body_id), 100.0, 0.0, 0.0),
		"over-budget impulse is rejected")
	var spent_before_node: float = float(simulation.get_mission_definition().delta_v_spent)
	var node_time: float = simulation.get_time() + 0.003
	check(simulation.schedule_impulse(int(definition.player_body_id), node_time, 0.0, 0.05, 0.0),
		"future maneuver node is accepted")
	check(bool(simulation.get_scheduled_impulse().active), "scheduled node is observable")
	check(not simulation.apply_impulse(int(definition.player_body_id), 0.0, 3.4, 0.0),
		"committed node reserves its delta-v budget")
	var snapshot: String = simulation.save_snapshot()
	var saved_time: float = simulation.get_time()
	var burn_report: Dictionary = simulation.advance(0.01)
	check(bool(burn_report.burn_executed), "scheduled node executes while advancing")
	check(not bool(simulation.get_scheduled_impulse().active), "executed node is cleared")
	check(float(simulation.get_mission_definition().delta_v_spent) > spent_before_node,
		"scheduled burn consumes delta-v budget")
	simulation.load_mission(4)
	check(simulation.load_snapshot(snapshot), "snapshot is restored through GDExtension")
	check(abs(simulation.get_time() - saved_time) < 1.0e-14, "snapshot restores exact time")
	check(int(simulation.get_mission_definition().index) == 0, "snapshot restores the mission identity")
	check(bool(simulation.get_scheduled_impulse().active), "snapshot restores pending maneuver node")
	check(abs(float(simulation.get_mission_definition().delta_v_spent) - spent_before_node) < 1.0e-14,
		"snapshot restores mission budget state")

	if failures == 0:
		print("PhyzBox Godot integration tests passed")
		quit(0)
	else:
		printerr("%d Godot integration test(s) failed" % failures)
		quit(1)
