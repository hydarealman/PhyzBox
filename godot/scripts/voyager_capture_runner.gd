extends SceneTree

func _init() -> void:
	call_deferred("_capture")

func _capture() -> void:
	var scene: PackedScene = load("res://scenes/voyager_main.tscn")
	var game := scene.instantiate()
	root.add_child(game)
	for frame in range(8):
		await process_frame
	await RenderingServer.frame_post_draw
	var root_path := ProjectSettings.globalize_path("res://../.runtime")
	DirAccess.make_dir_recursive_absolute(root_path)
	var launch_result := root.get_texture().get_image().save_png(root_path.path_join("voyager-launch.png"))
	game.call("_toggle_view")
	for frame in range(5):
		await process_frame
	await RenderingServer.frame_post_draw
	var map_result := root.get_texture().get_image().save_png(root_path.path_join("voyager-map.png"))
	game.call("_toggle_view")
	game.call("_event_selected", 1)
	for frame in range(5):
		await process_frame
	await RenderingServer.frame_post_draw
	var jupiter_result := root.get_texture().get_image().save_png(root_path.path_join("voyager-jupiter.png"))
	game.call("_event_selected", 2)
	for frame in range(5):
		await process_frame
	await RenderingServer.frame_post_draw
	var saturn_result := root.get_texture().get_image().save_png(root_path.path_join("voyager-saturn.png"))
	print("Voyager captures: ", launch_result, " ", map_result, " ", jupiter_result, " ", saturn_result)
	quit(0 if launch_result == OK and map_result == OK and jupiter_result == OK and saturn_result == OK else 1)
