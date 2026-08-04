extends Node3D

const Campaign = preload("res://scripts/campaign.gd")

const COLORS := [
	Color("ffbd66"), Color("6aaeff"), Color("9ee17a"),
	Color("b99cff"), Color("ff7b72"), Color("d7e8ff"), Color("f4fbff")
]
const PLAYER_COLOR := Color("65e6ff")
const TARGET_COLOR := Color("ff70c8")
const PRIMARY_COLOR := Color("ffc45e")
const TIME_RATES := [0.25, 1.0, 4.0, 12.0, 30.0]
const SAVE_PATH := "user://mission.snapshot"
const PROGRESS_PATH := "user://campaign.cfg"

var simulation
var current_mission := 0
var unlocked_mission := 0
var best_scores: Dictionary = {}
var mission_finished := false

var body_nodes: Dictionary = {}
var body_local_positions: Dictionary = {}
var trails: Dictionary = {}
var trail_meshes: Dictionary = {}
var prediction_mesh: MeshInstance3D
var target_line_mesh: MeshInstance3D
var selected_body_id := 0
var target_body_id := 0
var primary_body_id := 0
var running := false
var time_rate_index := 2

var camera: Camera3D
var camera_focus := Vector3.ZERO
var camera_focus_body_id := 0
var camera_distance := 4.2
var camera_yaw := 0.15
var camera_pitch := 0.58
var camera_dragging := false

var chapter_label: Label
var mission_title: Label
var objective_label: Label
var progress_label: Label
var telemetry_label: Label
var orbit_label: Label
var result_label: Label
var time_button: Button
var mission_picker: OptionButton
var impulse_x: LineEdit
var impulse_y: LineEdit
var impulse_z: LineEdit
var burn_time: LineEdit

var overlay: ColorRect
var overlay_chapter: Label
var overlay_title: Label
var overlay_story: Label
var overlay_objective: Label
var overlay_tutorial: Label
var overlay_button: Button
var overlay_mode := "briefing"

func _ready() -> void:
	simulation = PhyzSimulation.new()
	simulation.name = "PhysicsSimulation"
	add_child(simulation)
	_build_world()
	_build_ui()
	_load_campaign_progress()
	_apply_mission_locks()
	_load_mission(0)

func _build_world() -> void:
	camera = Camera3D.new()
	camera.name = "OrbitCamera"
	camera.fov = 54.0
	add_child(camera)

	var light := DirectionalLight3D.new()
	light.rotation_degrees = Vector3(-52.0, -28.0, 18.0)
	light.light_energy = 1.55
	add_child(light)

	var fill := OmniLight3D.new()
	fill.position = Vector3(0.0, 3.0, 2.0)
	fill.omni_range = 18.0
	fill.light_energy = 0.42
	fill.light_color = Color("88bfff")
	add_child(fill)

	prediction_mesh = MeshInstance3D.new()
	prediction_mesh.name = "PredictedTrajectory"
	add_child(prediction_mesh)

	target_line_mesh = MeshInstance3D.new()
	target_line_mesh.name = "TargetVector"
	add_child(target_line_mesh)

	var grid := MeshInstance3D.new()
	grid.name = "NavigationGrid"
	var grid_mesh := ImmediateMesh.new()
	grid_mesh.surface_begin(Mesh.PRIMITIVE_LINES)
	for i in range(-12, 13):
		var p := float(i) * 0.5
		grid_mesh.surface_add_vertex(Vector3(p, 0.0, -6.0))
		grid_mesh.surface_add_vertex(Vector3(p, 0.0, 6.0))
		grid_mesh.surface_add_vertex(Vector3(-6.0, 0.0, p))
		grid_mesh.surface_add_vertex(Vector3(6.0, 0.0, p))
	grid_mesh.surface_end()
	grid.mesh = grid_mesh
	var grid_material := StandardMaterial3D.new()
	grid_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	grid_material.albedo_color = Color(0.12, 0.32, 0.50, 0.20)
	grid_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	grid.material_override = grid_material
	add_child(grid)

	_build_starfield()
	_update_camera_transform()

func _build_starfield() -> void:
	var stars := MeshInstance3D.new()
	stars.name = "Starfield"
	var mesh := ImmediateMesh.new()
	var rng := RandomNumberGenerator.new()
	rng.seed = 2197
	mesh.surface_begin(Mesh.PRIMITIVE_POINTS)
	for index in range(420):
		var direction := Vector3(
			rng.randf_range(-1.0, 1.0),
			rng.randf_range(-0.8, 1.0),
			rng.randf_range(-1.0, 1.0)
		).normalized()
		mesh.surface_add_vertex(direction * rng.randf_range(18.0, 26.0))
	mesh.surface_end()
	stars.mesh = mesh
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_color = Color(0.70, 0.82, 1.0, 0.70)
	material.vertex_color_use_as_albedo = true
	material.point_size = 2.0
	stars.material_override = material
	add_child(stars)

func _build_ui() -> void:
	var layer := CanvasLayer.new()
	layer.name = "GameUI"
	add_child(layer)

	var panel := PanelContainer.new()
	panel.name = "MissionPanel"
	panel.set_anchors_preset(Control.PRESET_LEFT_WIDE)
	panel.offset_left = 14.0
	panel.offset_top = 14.0
	panel.offset_right = 430.0
	panel.offset_bottom = -14.0
	panel.add_theme_stylebox_override("panel", _panel_style(Color(0.025, 0.040, 0.070, 0.94), Color("294b68")))
	layer.add_child(panel)

	var scroll := ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	panel.add_child(scroll)

	var box := VBoxContainer.new()
	box.name = "MissionControls"
	box.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	box.add_theme_constant_override("separation", 8)
	scroll.add_child(box)

	var brand := Label.new()
	brand.text = "PHYZBOX // 余烬航线"
	brand.add_theme_font_size_override("font_size", 23)
	brand.modulate = Color("dcecff")
	box.add_child(brand)

	var subtitle := Label.new()
	subtitle.text = "真实引力轨道叙事游戏"
	subtitle.modulate = Color("7699b8")
	box.add_child(subtitle)

	mission_picker = OptionButton.new()
	mission_picker.name = "MissionPicker"
	mission_picker.tooltip_text = "已完成任务会解锁下一段航线"
	for index in range(Campaign.MISSIONS.size()):
		var data: Dictionary = Campaign.mission(index)
		mission_picker.add_item("%s  %s" % [data.code, data.title])
	mission_picker.item_selected.connect(_mission_selected)
	box.add_child(mission_picker)

	chapter_label = Label.new()
	chapter_label.modulate = Color("78d7ff")
	chapter_label.add_theme_font_size_override("font_size", 15)
	box.add_child(chapter_label)

	mission_title = Label.new()
	mission_title.add_theme_font_size_override("font_size", 22)
	mission_title.modulate = Color("f2f7ff")
	box.add_child(mission_title)

	objective_label = Label.new()
	objective_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	objective_label.custom_minimum_size.y = 48
	objective_label.modulate = Color("d4deeb")
	box.add_child(objective_label)

	progress_label = Label.new()
	progress_label.name = "MissionProgress"
	progress_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	progress_label.add_theme_font_size_override("font_size", 17)
	progress_label.modulate = Color("8ee6c1")
	box.add_child(progress_label)

	var nav_separator := HSeparator.new()
	box.add_child(nav_separator)

	var nav_title := Label.new()
	nav_title.text = "导航电脑 // 机动规划"
	nav_title.modulate = Color("ffc96b")
	nav_title.add_theme_font_size_override("font_size", 17)
	box.add_child(nav_title)

	var guidance_button := Button.new()
	guidance_button.name = "GuidanceButton"
	guidance_button.text = "应用导航建议（可行解）"
	guidance_button.tooltip_text = "填入经过自动测试的可行机动；你仍需预测、提交和执行"
	guidance_button.pressed.connect(_apply_guidance)
	guidance_button.add_to_group("playable_control")
	box.add_child(guidance_button)

	var maneuver_row := HBoxContainer.new()
	for axis in ["X", "Y", "Z"]:
		var column := VBoxContainer.new()
		column.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		var axis_label := Label.new()
		axis_label.text = "Δv %s" % axis
		axis_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		axis_label.modulate = Color("91a8bd")
		column.add_child(axis_label)
		var edit := LineEdit.new()
		edit.name = "Impulse%s" % axis
		edit.text = "0.000"
		edit.alignment = HORIZONTAL_ALIGNMENT_CENTER
		edit.tooltip_text = "速度增量，单位 AU/年"
		column.add_child(edit)
		maneuver_row.add_child(column)
		if axis == "X": impulse_x = edit
		elif axis == "Y": impulse_y = edit
		else: impulse_z = edit
	box.add_child(maneuver_row)

	var burn_time_row := HBoxContainer.new()
	var burn_time_label := Label.new()
	burn_time_label.text = "执行时刻（年）"
	burn_time_row.add_child(burn_time_label)
	burn_time = LineEdit.new()
	burn_time.text = "0.0000"
	burn_time.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	burn_time.tooltip_text = "绝对仿真时刻，不能早于当前时间"
	burn_time_row.add_child(burn_time)
	box.add_child(burn_time_row)

	var planning_buttons := HBoxContainer.new()
	var predict_button := _make_button("预测轨迹", "PredictButton", _predict_maneuver)
	planning_buttons.add_child(predict_button)
	var commit_button := _make_button("提交节点", "CommitButton", _commit_maneuver)
	planning_buttons.add_child(commit_button)
	var cancel_button := _make_button("取消节点", "CancelButton", _cancel_maneuver)
	planning_buttons.add_child(cancel_button)
	box.add_child(planning_buttons)

	var execution_buttons := HBoxContainer.new()
	time_button = _make_button("启动时间", "RunButton", _toggle_running)
	execution_buttons.add_child(time_button)
	execution_buttons.add_child(_make_button("立即点火", "BurnNowButton", _execute_maneuver))
	execution_buttons.add_child(_make_button("单步", "SingleStepButton", _single_step))
	box.add_child(execution_buttons)

	var time_buttons := HBoxContainer.new()
	time_buttons.add_child(_make_button("减速", "SlowerButton", _slower))
	var home_button := _make_button("全局视角", "HomeCameraButton", _focus_system)
	time_buttons.add_child(home_button)
	var target_button := _make_button("目标视角", "TargetCameraButton", _focus_target)
	time_buttons.add_child(target_button)
	time_buttons.add_child(_make_button("加速", "FasterButton", _faster))
	box.add_child(time_buttons)

	result_label = Label.new()
	result_label.name = "ActionFeedback"
	result_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	result_label.custom_minimum_size.y = 46
	result_label.modulate = Color("8ee6c1")
	box.add_child(result_label)

	telemetry_label = Label.new()
	telemetry_label.modulate = Color("9db1c5")
	telemetry_label.add_theme_font_size_override("font_size", 14)
	box.add_child(telemetry_label)

	orbit_label = Label.new()
	orbit_label.modulate = Color("71869a")
	orbit_label.add_theme_font_size_override("font_size", 13)
	box.add_child(orbit_label)

	var persistence_row := HBoxContainer.new()
	persistence_row.add_child(_make_button("保存进度", "SaveButton", _save_snapshot))
	persistence_row.add_child(_make_button("读取进度", "LoadButton", _load_snapshot))
	persistence_row.add_child(_make_button("重开任务", "RestartButton", _restart_mission))
	box.add_child(persistence_row)

	var help := Label.new()
	help.text = "拖动空白处旋转 · 滚轮缩放 · F飞船 · T目标 · H全局 · 空格暂停"
	help.modulate = Color("667f96")
	help.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	box.add_child(help)

	_build_story_overlay(layer)

func _build_story_overlay(layer: CanvasLayer) -> void:
	overlay = ColorRect.new()
	overlay.name = "StoryOverlay"
	overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	overlay.color = Color(0.005, 0.010, 0.025, 0.84)
	overlay.mouse_filter = Control.MOUSE_FILTER_STOP
	layer.add_child(overlay)

	var card := PanelContainer.new()
	card.set_anchors_preset(Control.PRESET_CENTER)
	card.offset_left = -330.0
	card.offset_top = -235.0
	card.offset_right = 330.0
	card.offset_bottom = 235.0
	card.add_theme_stylebox_override("panel", _panel_style(Color(0.025, 0.050, 0.085, 0.98), Color("3a789e")))
	overlay.add_child(card)

	var content := VBoxContainer.new()
	content.add_theme_constant_override("separation", 14)
	card.add_child(content)

	var campaign_name := Label.new()
	campaign_name.text = Campaign.CAMPAIGN_TITLE
	campaign_name.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	campaign_name.add_theme_font_size_override("font_size", 16)
	campaign_name.modulate = Color("78d7ff")
	content.add_child(campaign_name)

	overlay_chapter = Label.new()
	overlay_chapter.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	overlay_chapter.modulate = Color("91a8bd")
	content.add_child(overlay_chapter)

	overlay_title = Label.new()
	overlay_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	overlay_title.add_theme_font_size_override("font_size", 29)
	overlay_title.modulate = Color("f3f7ff")
	content.add_child(overlay_title)

	overlay_story = Label.new()
	overlay_story.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	overlay_story.custom_minimum_size.y = 108
	overlay_story.add_theme_font_size_override("font_size", 17)
	overlay_story.modulate = Color("cbd8e5")
	content.add_child(overlay_story)

	overlay_objective = Label.new()
	overlay_objective.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	overlay_objective.modulate = Color("8ee6c1")
	overlay_objective.add_theme_font_size_override("font_size", 18)
	content.add_child(overlay_objective)

	overlay_tutorial = Label.new()
	overlay_tutorial.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	overlay_tutorial.modulate = Color("ffc96b")
	content.add_child(overlay_tutorial)

	overlay_button = Button.new()
	overlay_button.name = "StoryActionButton"
	overlay_button.custom_minimum_size.y = 48
	overlay_button.pressed.connect(_overlay_action_pressed)
	content.add_child(overlay_button)

func _make_button(text: String, name: String, callback: Callable) -> Button:
	var button := Button.new()
	button.name = name
	button.text = text
	button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	button.pressed.connect(callback)
	button.add_to_group("playable_control")
	return button

func _panel_style(background: Color, border: Color) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = background
	style.border_color = border
	style.set_border_width_all(1)
	style.set_corner_radius_all(8)
	style.content_margin_left = 14.0
	style.content_margin_right = 14.0
	style.content_margin_top = 12.0
	style.content_margin_bottom = 12.0
	return style

func _process(delta: float) -> void:
	if running and not mission_finished:
		var years := delta * 0.03 * float(TIME_RATES[time_rate_index])
		var report: Dictionary = simulation.advance(years)
		if bool(report.get("burn_executed", false)):
			result_label.text = "机动节点已执行。正在沿新轨迹推进。"
	_update_bodies()
	_update_telemetry()
	_update_camera(delta)

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT or event.button_index == MOUSE_BUTTON_RIGHT:
			camera_dragging = event.pressed
		elif event.pressed and event.button_index == MOUSE_BUTTON_WHEEL_UP:
			camera_distance = max(1.2, camera_distance * 0.86)
		elif event.pressed and event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			camera_distance = min(24.0, camera_distance * 1.16)
	elif event is InputEventMouseMotion and camera_dragging:
		camera_yaw -= event.relative.x * 0.007
		camera_pitch = clamp(camera_pitch + event.relative.y * 0.006, 0.12, 1.35)
	elif event is InputEventKey and event.pressed and not event.echo:
		match event.physical_keycode:
			KEY_SPACE: _toggle_running()
			KEY_EQUAL, KEY_KP_ADD: _faster()
			KEY_MINUS, KEY_KP_SUBTRACT: _slower()
			KEY_F: _focus_player()
			KEY_T: _focus_target()
			KEY_H: _focus_system()

func _mission_selected(index: int) -> void:
	if index > unlocked_mission:
		mission_picker.select(current_mission)
		result_label.text = "该任务尚未解锁。完成当前航线后继续。"
		return
	_load_mission(index)

func _load_mission(index: int, show_briefing := true) -> void:
	current_mission = clampi(index, 0, Campaign.MISSIONS.size() - 1)
	running = false
	mission_finished = false
	if time_button:
		time_button.text = "启动时间"
	simulation.load_mission(current_mission)
	var definition: Dictionary = simulation.get_mission_definition()
	selected_body_id = int(definition.player_body_id)
	target_body_id = int(definition.target_body_id)
	primary_body_id = int(definition.primary_body_id)
	camera_focus_body_id = primary_body_id
	burn_time.text = "%.4f" % simulation.get_time()
	_set_impulse(Vector3.ZERO)
	mission_picker.select(current_mission)
	_clear_visuals()
	_update_mission_text()
	_update_bodies()
	_fit_camera_to_system()
	_predict_maneuver(false)
	result_label.text = "先查看任务简报，再使用导航建议完成第一次规划。"
	if show_briefing:
		_show_briefing()

func _update_mission_text() -> void:
	var mission: Dictionary = Campaign.mission(current_mission)
	var chapter: Dictionary = Campaign.chapter(int(mission.chapter))
	chapter_label.text = str(chapter.title)
	mission_title.text = "%s  %s" % [mission.code, mission.title]
	objective_label.text = "任务目标：%s" % mission.objective

func _show_briefing() -> void:
	var mission: Dictionary = Campaign.mission(current_mission)
	var chapter: Dictionary = Campaign.chapter(int(mission.chapter))
	overlay_mode = "briefing"
	overlay_chapter.text = str(chapter.title)
	overlay_title.text = "%s // %s" % [mission.code, mission.title]
	overlay_story.text = "%s\n\n%s" % [chapter.summary, mission.story]
	overlay_objective.text = "目标：%s" % mission.objective
	overlay_tutorial.text = "导航核心：%s" % mission.tutorial
	overlay_button.text = "进入任务"
	overlay.visible = true

func _show_debrief(success: bool, score := 0.0) -> void:
	var mission: Dictionary = Campaign.mission(current_mission)
	overlay_chapter.text = "任务结算"
	overlay_title.text = "完成：%s" % mission.title if success else "任务失败：%s" % mission.title
	overlay_story.text = str(mission.success if success else mission.failure)
	overlay_objective.text = "得分 %.1f" % score if success else "你可以读取快照或从任务起点重新规划。"
	overlay_tutorial.text = "更少的delta-v会得到更高评分。" if success else "黄色预测线是独立物理分支，不会改写当前状态。"
	if success and current_mission + 1 < Campaign.MISSIONS.size():
		overlay_mode = "next"
		overlay_button.text = "进入下一任务"
	elif success:
		overlay_mode = "finale"
		overlay_button.text = "返回星图"
	else:
		overlay_mode = "retry"
		overlay_button.text = "重新开始"
	overlay.visible = true

func _overlay_action_pressed() -> void:
	match overlay_mode:
		"next":
			overlay.visible = false
			_load_mission(current_mission + 1)
		"retry":
			overlay.visible = false
			_load_mission(current_mission)
		_:
			overlay.visible = false
			result_label.text = "点击“导航建议”，再点击“预测轨迹”查看可行路线。"

func _apply_guidance() -> void:
	if simulation.get_time() > 1.0e-8:
		result_label.text = "导航建议以任务起点为基准。请点击“重开任务”后再应用。"
		return
	var mission: Dictionary = Campaign.mission(current_mission)
	var hint: Vector3 = mission.hint
	_set_impulse(hint)
	burn_time.text = "%.4f" % float(mission.hint_time)
	_predict_maneuver(false)
	if hint.is_zero_approx():
		result_label.text = "导航建议：暂不点火，先观察自然演化，然后启动时间。"
	else:
		result_label.text = "已填入可行机动。黄色线是预测结果；确认后点击“提交节点”。"

func _clear_visuals() -> void:
	for node in body_nodes.values():
		node.queue_free()
	for node in trail_meshes.values():
		node.queue_free()
	body_nodes.clear()
	body_local_positions.clear()
	trail_meshes.clear()
	trails.clear()
	prediction_mesh.mesh = null
	target_line_mesh.mesh = null

func _update_bodies() -> void:
	var bodies: Array = simulation.get_bodies()
	var origin := Vector3.ZERO
	for data in bodies:
		if int(data.id) == primary_body_id:
			origin = _v3(data.position)
	for data in bodies:
		var id := int(data.id)
		if not body_nodes.has(id):
			_create_body_node(data)
		var local_position := _v3(data.position) - origin
		body_local_positions[id] = local_position
		body_nodes[id].position = local_position
		if not trails.has(id):
			trails[id] = PackedVector3Array()
		var points: PackedVector3Array = trails[id]
		if points.is_empty() or points[points.size() - 1].distance_to(local_position) > 0.003:
			points.append(local_position)
			if points.size() > 560:
				points.remove_at(0)
			trails[id] = points
			var trail_color := PLAYER_COLOR if id == selected_body_id else Color(0.30, 0.62, 0.92, 0.40)
			_update_line_mesh(trail_meshes[id], points, trail_color)
	_update_target_vector()

func _create_body_node(data: Dictionary) -> void:
	var id := int(data.id)
	var holder := Node3D.new()
	holder.name = "Body_%s" % id
	add_child(holder)
	body_nodes[id] = holder

	var radius: float = clampf(float(data.display_radius) * 1.45, 0.045, 0.16)
	if id == selected_body_id:
		radius = max(radius, 0.085)
	elif id == target_body_id:
		radius = max(radius, 0.10)
	elif id == primary_body_id:
		radius = max(radius, 0.13)

	var sphere := SphereMesh.new()
	sphere.radius = radius
	sphere.height = radius * 2.0
	sphere.radial_segments = 28
	sphere.rings = 16
	var mesh_instance := MeshInstance3D.new()
	mesh_instance.mesh = sphere
	var material := StandardMaterial3D.new()
	var color: Color = COLORS[int(data.kind) % COLORS.size()]
	if id == selected_body_id: color = PLAYER_COLOR
	elif id == target_body_id: color = TARGET_COLOR
	elif id == primary_body_id: color = PRIMARY_COLOR
	material.albedo_color = color
	material.emission_enabled = true
	material.emission = color * (1.25 if id == primary_body_id else 0.42)
	mesh_instance.material_override = material
	holder.add_child(mesh_instance)

	var role := ""
	if id == selected_body_id: role = "  [你的飞船]"
	elif id == target_body_id: role = "  [任务目标]"
	elif id == primary_body_id: role = "  [主天体]"
	var label := Label3D.new()
	label.text = "%s%s" % [str(data.name), role]
	label.position = Vector3(0.0, radius + 0.085, 0.0)
	label.font_size = 28
	label.outline_size = 7
	label.pixel_size = 0.0027
	label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	label.no_depth_test = true
	label.modulate = color.lightened(0.18)
	holder.add_child(label)

	if id == selected_body_id or id == target_body_id:
		var marker_color := PLAYER_COLOR if id == selected_body_id else TARGET_COLOR
		holder.add_child(_make_marker(radius + 0.075, marker_color))

	var trail := MeshInstance3D.new()
	trail.name = "Trail_%s" % id
	add_child(trail)
	trail_meshes[id] = trail

func _make_marker(radius: float, color: Color) -> MeshInstance3D:
	var marker := MeshInstance3D.new()
	var mesh := ImmediateMesh.new()
	mesh.surface_begin(Mesh.PRIMITIVE_LINE_STRIP)
	for index in range(49):
		var angle := TAU * float(index) / 48.0
		mesh.surface_add_vertex(Vector3(cos(angle) * radius, 0.006, sin(angle) * radius))
	mesh.surface_end()
	marker.mesh = mesh
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_color = color
	material.emission_enabled = true
	material.emission = color
	marker.material_override = material
	return marker

func _update_target_vector() -> void:
	if not body_local_positions.has(selected_body_id) or not body_local_positions.has(target_body_id):
		return
	var points := PackedVector3Array([
		body_local_positions[selected_body_id],
		body_local_positions[target_body_id],
	])
	_update_line_mesh(target_line_mesh, points, Color(1.0, 0.32, 0.70, 0.30))

func _update_line_mesh(instance: MeshInstance3D, points: PackedVector3Array, color: Color) -> void:
	if points.size() < 2:
		instance.mesh = null
		return
	var immediate := ImmediateMesh.new()
	immediate.surface_begin(Mesh.PRIMITIVE_LINE_STRIP)
	for point in points:
		immediate.surface_add_vertex(point)
	immediate.surface_end()
	instance.mesh = immediate
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_color = color
	material.emission_enabled = true
	material.emission = Color(color.r, color.g, color.b)
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	instance.material_override = material

func _predict_maneuver(show_message := true) -> void:
	if selected_body_id == 0:
		return
	var definition: Dictionary = simulation.get_mission_definition()
	var duration: float = maxf(0.08, float(definition.deadline) - simulation.get_time())
	var node_time: float = maxf(simulation.get_time(), _number(burn_time))
	var prediction: Array = simulation.predict(
		selected_body_id,
		_number(impulse_x), _number(impulse_y), _number(impulse_z),
		node_time, duration, max(0.002, duration / 280.0)
	)
	var points := PackedVector3Array()
	var origin := Vector3.ZERO
	for data in simulation.get_bodies():
		if int(data.id) == primary_body_id:
			origin = _v3(data.position)
	for item in prediction:
		points.append(_v3(item.position) - origin)
	_update_line_mesh(prediction_mesh, points, Color(1.0, 0.78, 0.20, 0.92))
	if show_message:
		result_label.text = "预测完成：黄色轨迹包含%d个采样点。" % points.size()

func _execute_maneuver() -> void:
	var accepted: bool = simulation.apply_impulse(
		selected_body_id, _number(impulse_x), _number(impulse_y), _number(impulse_z)
	)
	result_label.text = "立即点火完成。" if accepted else "点火被拒绝：燃料不足、数值无效或已有节点占用预算。"
	if accepted:
		_predict_maneuver(false)

func _commit_maneuver() -> void:
	var accepted: bool = simulation.schedule_impulse(
		selected_body_id, _number(burn_time),
		_number(impulse_x), _number(impulse_y), _number(impulse_z)
	)
	result_label.text = "机动节点已提交。启动时间后会在指定时刻自动执行。" if accepted else "节点无效：检查执行时刻和delta-v预算。"

func _cancel_maneuver() -> void:
	result_label.text = "已取消机动节点。" if simulation.cancel_scheduled_impulse() else "当前没有待执行节点。"

func _update_telemetry() -> void:
	var definition: Dictionary = simulation.get_mission_definition()
	var evaluation: Dictionary = simulation.evaluate_mission()
	var scheduled: Dictionary = simulation.get_scheduled_impulse()
	var node_status := "无"
	if bool(scheduled.get("active", false)):
		node_status = "%.4f年" % float(scheduled.time)
	var reserved := float(definition.get("delta_v_reserved", 0.0))
	telemetry_label.text = "时间 %.4f / %.2f年   倍速 %.2fx\n燃料 %.3f已用 + %.3f预留 / %.3f AU/年   节点 %s\n%s · 固定步长 %s年" % [
		simulation.get_time(), float(definition.deadline), float(TIME_RATES[time_rate_index]),
		float(definition.delta_v_spent), reserved, float(definition.delta_v_budget), node_status,
		simulation.get_integrator_name(), String.num_scientific(simulation.get_fixed_step())
	]
	_update_progress_text(evaluation)
	var elements: Dictionary = simulation.get_orbital_elements(selected_body_id, primary_body_id)
	if bool(elements.get("valid", false)):
		orbit_label.text = "轨道诊断  a %.3f AU · e %.4f · i %.2f° · 近心 %.3f · 远心 %.3f" % [
			float(elements.semi_major_axis), float(elements.eccentricity),
			rad_to_deg(float(elements.inclination)), float(elements.periapsis), float(elements.apoapsis)
		]
	if mission_finished:
		return
	if bool(evaluation.success):
		_complete_mission(float(evaluation.score))
	elif bool(evaluation.failed):
		_fail_mission()

func _update_progress_text(evaluation: Dictionary) -> void:
	match current_mission:
		0:
			progress_label.text = "目标距离 %.3f AU  /  需要 < 0.120" % float(evaluation.distance)
		1:
			progress_label.text = "距离 %.3f AU  /  相对速度 %.3f AU/年" % [
				float(evaluation.distance), float(evaluation.relative_speed)
			]
		2:
			progress_label.text = "速度保留 %.1f%%  /  掠过后需要 > 96%%" % (float(evaluation.speed_ratio) * 100.0)
		3:
			progress_label.text = "威胁距离 %.3f AU  /  截止时需要 > 0.250" % float(evaluation.distance)
		4:
			progress_label.text = "观测存活 %.3f / 1.400 年" % simulation.get_time()

func _complete_mission(score: float) -> void:
	mission_finished = true
	running = false
	time_button.text = "启动时间"
	best_scores[current_mission] = maxf(float(best_scores.get(current_mission, 0.0)), score)
	if current_mission >= unlocked_mission and current_mission + 1 < Campaign.MISSIONS.size():
		unlocked_mission = current_mission + 1
	_apply_mission_locks()
	_save_campaign_progress()
	result_label.text = "任务完成，得分 %.1f。" % score
	_show_debrief(true, score)

func _fail_mission() -> void:
	mission_finished = true
	running = false
	time_button.text = "启动时间"
	result_label.text = "任务失败。读取快照或重新开始。"
	_show_debrief(false)

func _toggle_running() -> void:
	if overlay.visible or mission_finished:
		return
	running = not running
	time_button.text = "暂停时间" if running else "启动时间"

func _single_step() -> void:
	if not mission_finished:
		simulation.advance(simulation.get_fixed_step())

func _faster() -> void:
	time_rate_index = mini(time_rate_index + 1, TIME_RATES.size() - 1)
	result_label.text = "时间倍率 %.2fx" % TIME_RATES[time_rate_index]

func _slower() -> void:
	time_rate_index = maxi(time_rate_index - 1, 0)
	result_label.text = "时间倍率 %.2fx" % TIME_RATES[time_rate_index]

func _save_snapshot() -> void:
	var file := FileAccess.open(SAVE_PATH, FileAccess.WRITE)
	if file:
		file.store_string(simulation.save_snapshot())
		result_label.text = "任务快照已保存。"
	else:
		result_label.text = "无法写入任务快照。"

func _load_snapshot() -> void:
	var file := FileAccess.open(SAVE_PATH, FileAccess.READ)
	if file and simulation.load_snapshot(file.get_as_text()):
		var definition: Dictionary = simulation.get_mission_definition()
		current_mission = int(definition.index)
		unlocked_mission = maxi(unlocked_mission, current_mission)
		selected_body_id = int(definition.player_body_id)
		target_body_id = int(definition.target_body_id)
		primary_body_id = int(definition.primary_body_id)
		camera_focus_body_id = primary_body_id
		mission_picker.select(current_mission)
		mission_finished = false
		running = false
		_update_mission_text()
		var scheduled: Dictionary = simulation.get_scheduled_impulse()
		if bool(scheduled.get("active", false)):
			burn_time.text = "%.4f" % float(scheduled.time)
			var delta_values = scheduled.delta_velocity
			_set_impulse(Vector3(float(delta_values[0]), float(delta_values[1]), float(delta_values[2])))
		_clear_visuals()
		_update_bodies()
		_predict_maneuver(false)
		result_label.text = "任务快照已恢复。"
	else:
		result_label.text = "没有找到有效任务快照。"

func _restart_mission() -> void:
	_load_mission(current_mission)

func _load_campaign_progress() -> void:
	var config := ConfigFile.new()
	if config.load(PROGRESS_PATH) != OK:
		return
	unlocked_mission = clampi(int(config.get_value("campaign", "unlocked_mission", 0)), 0, Campaign.MISSIONS.size() - 1)
	for index in range(Campaign.MISSIONS.size()):
		var score := float(config.get_value("scores", str(index), 0.0))
		if score > 0.0:
			best_scores[index] = score

func _save_campaign_progress() -> void:
	var config := ConfigFile.new()
	config.set_value("campaign", "unlocked_mission", unlocked_mission)
	for key in best_scores:
		config.set_value("scores", str(key), best_scores[key])
	config.save(PROGRESS_PATH)

func _apply_mission_locks() -> void:
	if not mission_picker:
		return
	for index in range(Campaign.MISSIONS.size()):
		mission_picker.set_item_disabled(index, index > unlocked_mission)
		var data: Dictionary = Campaign.mission(index)
		var prefix := "✓" if best_scores.has(index) else ("◆" if index <= unlocked_mission else "🔒")
		mission_picker.set_item_text(index, "%s  %s  %s" % [prefix, data.code, data.title])

func _focus_player() -> void:
	camera_focus_body_id = selected_body_id
	result_label.text = "相机正在跟随你的飞船。"

func _focus_target() -> void:
	camera_focus_body_id = target_body_id
	result_label.text = "相机正在跟随任务目标。"

func _focus_system() -> void:
	camera_focus_body_id = primary_body_id
	_fit_camera_to_system()
	result_label.text = "已切换到全局导航视角。"

func _fit_camera_to_system() -> void:
	var extent := 1.0
	for position in body_local_positions.values():
		extent = maxf(extent, Vector3(position).length())
	camera_distance = clamp(extent * 2.2, 3.0, 14.0)
	camera_yaw = 0.18
	camera_pitch = 0.62

func _update_camera(delta: float) -> void:
	var target := Vector3.ZERO
	if body_local_positions.has(camera_focus_body_id):
		target = body_local_positions[camera_focus_body_id]
	camera_focus = camera_focus.lerp(target, clamp(delta * 8.0, 0.0, 1.0))
	_update_camera_transform()

func _update_camera_transform() -> void:
	var horizontal := cos(camera_pitch) * camera_distance
	var offset := Vector3(
		sin(camera_yaw) * horizontal,
		sin(camera_pitch) * camera_distance,
		cos(camera_yaw) * horizontal
	)
	camera.position = camera_focus + offset
	camera.look_at(camera_focus, Vector3.UP)

func _set_impulse(value: Vector3) -> void:
	impulse_x.text = "%.3f" % value.x
	impulse_y.text = "%.3f" % value.y
	impulse_z.text = "%.3f" % value.z

func _number(edit: LineEdit) -> float:
	return edit.text.to_float() if edit else 0.0

func _v3(values) -> Vector3:
	return Vector3(float(values[0]), float(values[2]), -float(values[1]))
