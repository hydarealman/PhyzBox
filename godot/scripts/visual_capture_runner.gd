extends SceneTree

func _init() -> void:
	call_deferred("_capture")

func _capture() -> void:
	var scene: PackedScene = load("res://scenes/main.tscn")
	var game := scene.instantiate()
	root.add_child(game)
	for frame in range(6):
		await process_frame
	await RenderingServer.frame_post_draw
	var briefing_output := ProjectSettings.globalize_path("res://../.runtime/briefing-preview.png")
	DirAccess.make_dir_recursive_absolute(briefing_output.get_base_dir())
	var briefing_result := root.get_texture().get_image().save_png(briefing_output)
	print("Briefing capture: ", briefing_output, " result=", briefing_result)
	var action := game.find_child("StoryActionButton", true, false) as Button
	action.pressed.emit()
	var thrust_event := InputEventKey.new()
	thrust_event.physical_keycode = KEY_W
	thrust_event.pressed = true
	Input.parse_input_event(thrust_event)
	var boost_event := InputEventKey.new()
	boost_event.physical_keycode = KEY_SHIFT
	boost_event.pressed = true
	Input.parse_input_event(boost_event)
	for frame in range(8):
		await process_frame
	await RenderingServer.frame_post_draw
	var output := ProjectSettings.globalize_path("res://../.runtime/ui-preview.png")
	DirAccess.make_dir_recursive_absolute(output.get_base_dir())
	var result := root.get_texture().get_image().save_png(output)
	print("UI capture: ", output, " result=", result)
	thrust_event.pressed = false
	Input.parse_input_event(thrust_event)
	boost_event.pressed = false
	Input.parse_input_event(boost_event)
	game.call("_set_view_mode", "map")
	for frame in range(5):
		await process_frame
	await RenderingServer.frame_post_draw
	var map_output := ProjectSettings.globalize_path("res://../.runtime/map-preview.png")
	var map_result := root.get_texture().get_image().save_png(map_output)
	print("Map capture: ", map_output, " result=", map_result)
	quit(0 if result == OK and briefing_result == OK and map_result == OK else 1)
