extends SceneTree

func _init() -> void:
	call_deferred("_capture")

func _capture() -> void:
	var scene: PackedScene = load("res://scenes/main.tscn")
	var game := scene.instantiate()
	root.add_child(game)
	for frame in range(6):
		await process_frame
	var action := game.find_child("StoryActionButton", true, false) as Button
	var guidance := game.find_child("GuidanceButton", true, false) as Button
	action.pressed.emit()
	guidance.pressed.emit()
	for frame in range(12):
		await process_frame
	await RenderingServer.frame_post_draw
	var output := ProjectSettings.globalize_path("res://../.runtime/ui-preview.png")
	DirAccess.make_dir_recursive_absolute(output.get_base_dir())
	var result := root.get_texture().get_image().save_png(output)
	print("UI capture: ", output, " result=", result)
	quit(0 if result == OK else 1)
