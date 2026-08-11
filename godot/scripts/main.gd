extends Node3D

const Campaign = preload("res://scripts/campaign.gd")

const COLORS := [
	Color("ffbd66"), Color("6aaeff"), Color("9ee17a"),
	Color("b99cff"), Color("ff7b72"), Color("d7e8ff"), Color("f4fbff")
]
const PLAYER_COLOR := Color("65e6ff")
const TARGET_COLOR := Color("ff70c8")
const PRIMARY_COLOR := Color("ffc45e")
const INK := Color("081018")
const PANEL := Color(0.025, 0.045, 0.070, 0.88)
const PANEL_SOLID := Color("0b1520")
const TEXT_MAIN := Color("e7f4ff")
const TEXT_MUTED := Color("7891a6")
const ACCENT := Color("5ce1e6")
const AMBER := Color("ffb454")
const TIME_RATES := [0.25, 1.0, 4.0, 12.0, 30.0]
const SAVE_PATH := "user://mission.snapshot"
const PROGRESS_PATH := "user://campaign.cfg"
const BRIEFING_SHADER = preload("res://shaders/briefing_space.gdshader")
const PLANET_SHADER = preload("res://shaders/planet_surface.gdshader")

var simulation
var current_mission := 0
var unlocked_mission := 0
var best_scores: Dictionary = {}
var mission_finished := false

var body_nodes: Dictionary = {}
var body_visuals: Dictionary = {}
var body_local_positions: Dictionary = {}
var body_local_velocities: Dictionary = {}
var trails: Dictionary = {}
var trail_meshes: Dictionary = {}
var prediction_mesh: MeshInstance3D
var target_line_mesh: MeshInstance3D
var navigation_grid: MeshInstance3D
var selected_body_id := 0
var target_body_id := 0
var primary_body_id := 0
var running := false
var time_rate_index := 1
var view_mode := "flight"
var current_throttle := 0.0
var last_thrust_direction := Vector3.ZERO
var guidance_enabled := true

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
var fuel_bar: ProgressBar
var time_bar: ProgressBar
var telemetry_panel: PanelContainer
var telemetry_toggle: Button
var command_dock: PanelContainer
var flight_hud: PanelContainer
var flight_status_label: Label
var flight_controls_label: Label
var flight_reticle: Label
var view_toggle: Button
var impulse_x: LineEdit
var impulse_y: LineEdit
var impulse_z: LineEdit
var burn_time: LineEdit

var overlay: ColorRect
var overlay_backdrop: ColorRect
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
	var world_environment := WorldEnvironment.new()
	world_environment.name = "CinematicEnvironment"
	var environment := Environment.new()
	environment.background_mode = Environment.BG_COLOR
	environment.background_color = Color("02050a")
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.ambient_light_color = Color("29435f")
	environment.ambient_light_energy = 0.38
	environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	world_environment.environment = environment
	add_child(world_environment)

	camera = Camera3D.new()
	camera.name = "OrbitCamera"
	camera.fov = 49.0
	camera.near = 0.008
	add_child(camera)

	var light := DirectionalLight3D.new()
	light.rotation_degrees = Vector3(-52.0, -28.0, 18.0)
	light.light_energy = 1.85
	light.shadow_enabled = true
	add_child(light)

	var fill := OmniLight3D.new()
	fill.position = Vector3(0.0, 3.0, 2.0)
	fill.omni_range = 18.0
	fill.light_energy = 0.58
	fill.light_color = Color("88bfff")
	add_child(fill)

	prediction_mesh = MeshInstance3D.new()
	prediction_mesh.name = "PredictedTrajectory"
	add_child(prediction_mesh)

	target_line_mesh = MeshInstance3D.new()
	target_line_mesh.name = "TargetVector"
	add_child(target_line_mesh)

	navigation_grid = MeshInstance3D.new()
	navigation_grid.name = "NavigationGrid"
	var grid_mesh := ImmediateMesh.new()
	grid_mesh.surface_begin(Mesh.PRIMITIVE_LINES)
	for i in range(-10, 11):
		var p := float(i) * 0.5
		grid_mesh.surface_add_vertex(Vector3(p, -0.012, -5.0))
		grid_mesh.surface_add_vertex(Vector3(p, -0.012, 5.0))
		grid_mesh.surface_add_vertex(Vector3(-5.0, -0.012, p))
		grid_mesh.surface_add_vertex(Vector3(5.0, -0.012, p))
	grid_mesh.surface_end()
	navigation_grid.mesh = grid_mesh
	var grid_material := StandardMaterial3D.new()
	grid_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	grid_material.albedo_color = Color(0.08, 0.32, 0.46, 0.10)
	grid_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	navigation_grid.material_override = grid_material
	add_child(navigation_grid)

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

	var top_bar := PanelContainer.new()
	top_bar.set_anchors_preset(Control.PRESET_TOP_WIDE)
	top_bar.offset_left = 18.0
	top_bar.offset_top = 16.0
	top_bar.offset_right = -18.0
	top_bar.offset_bottom = 78.0
	top_bar.add_theme_stylebox_override("panel", _panel_style(Color(0.018, 0.032, 0.050, 0.91), Color(0.18, 0.45, 0.58, 0.55), 10))
	layer.add_child(top_bar)

	var top_row := HBoxContainer.new()
	top_row.add_theme_constant_override("separation", 18)
	top_bar.add_child(top_row)

	var brand := Label.new()
	brand.text = "PHYZBOX"
	brand.custom_minimum_size.x = 180.0
	brand.add_theme_font_size_override("font_size", 22)
	brand.modulate = TEXT_MAIN
	top_row.add_child(brand)

	var route_mark := Label.new()
	route_mark.text = "EMBER ROUTE  /  余烬航线"
	route_mark.modulate = ACCENT
	route_mark.add_theme_font_size_override("font_size", 13)
	top_row.add_child(route_mark)

	var top_spacer := Control.new()
	top_spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	top_row.add_child(top_spacer)

	view_toggle = _make_button("M  星图规划", "ViewToggle", _toggle_view_mode)
	view_toggle.custom_minimum_size.x = 150.0
	view_toggle.tooltip_text = "在直接驾驶与轨道规划之间切换"
	top_row.add_child(view_toggle)

	mission_picker = OptionButton.new()
	mission_picker.name = "MissionPicker"
	mission_picker.custom_minimum_size = Vector2(265.0, 38.0)
	mission_picker.tooltip_text = "航线任务 / 已完成任务会解锁下一段"
	for index in range(Campaign.MISSIONS.size()):
		var picker_data: Dictionary = Campaign.mission(index)
		mission_picker.add_item("%s  %s" % [picker_data.code, picker_data.title])
	mission_picker.item_selected.connect(_mission_selected)
	top_row.add_child(mission_picker)

	var panel := PanelContainer.new()
	panel.name = "MissionPanel"
	panel.position = Vector2(22.0, 96.0)
	panel.size = Vector2(410.0, 150.0)
	panel.add_theme_stylebox_override("panel", _panel_style(PANEL, Color(0.15, 0.46, 0.60, 0.72), 12))
	layer.add_child(panel)

	var mission_box := VBoxContainer.new()
	mission_box.add_theme_constant_override("separation", 4)
	panel.add_child(mission_box)

	chapter_label = Label.new()
	chapter_label.modulate = ACCENT
	chapter_label.add_theme_font_size_override("font_size", 12)
	mission_box.add_child(chapter_label)

	mission_title = Label.new()
	mission_title.add_theme_font_size_override("font_size", 25)
	mission_title.modulate = TEXT_MAIN
	mission_box.add_child(mission_title)

	objective_label = Label.new()
	objective_label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	objective_label.modulate = TEXT_MUTED
	mission_box.add_child(objective_label)

	progress_label = Label.new()
	progress_label.name = "MissionProgress"
	progress_label.add_theme_font_size_override("font_size", 16)
	progress_label.modulate = Color("83f3c5")
	mission_box.add_child(progress_label)

	var bar_row := HBoxContainer.new()
	bar_row.add_theme_constant_override("separation", 12)
	mission_box.add_child(bar_row)
	time_bar = _make_meter("T", Color("53c7ff"))
	bar_row.add_child(time_bar.get_parent())
	fuel_bar = _make_meter("ΔV", AMBER)
	bar_row.add_child(fuel_bar.get_parent())

	result_label = Label.new()
	result_label.name = "ActionFeedback"
	result_label.set_anchors_preset(Control.PRESET_CENTER_BOTTOM)
	result_label.offset_left = -400.0
	result_label.offset_top = -260.0
	result_label.offset_right = 400.0
	result_label.offset_bottom = -224.0
	result_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	result_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	result_label.add_theme_font_size_override("font_size", 14)
	result_label.modulate = Color("9cefd3")
	layer.add_child(result_label)

	command_dock = PanelContainer.new()
	command_dock.name = "CommandDock"
	command_dock.set_anchors_preset(Control.PRESET_CENTER_BOTTOM)
	command_dock.offset_left = -455.0
	command_dock.offset_top = -215.0
	command_dock.offset_right = 455.0
	command_dock.offset_bottom = -20.0
	command_dock.add_theme_stylebox_override("panel", _panel_style(Color(0.018, 0.032, 0.048, 0.94), Color(0.19, 0.50, 0.62, 0.72), 14))
	layer.add_child(command_dock)

	var dock_box := VBoxContainer.new()
	dock_box.add_theme_constant_override("separation", 9)
	command_dock.add_child(dock_box)

	var dock_header := HBoxContainer.new()
	dock_box.add_child(dock_header)
	var nav_title := Label.new()
	nav_title.text = "MANEUVER NODE"
	nav_title.modulate = AMBER
	nav_title.add_theme_font_size_override("font_size", 13)
	dock_header.add_child(nav_title)
	var dock_spacer := Control.new()
	dock_spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	dock_header.add_child(dock_spacer)
	var unit_label := Label.new()
	unit_label.text = "Δv  AU/yr   ·   epoch  yr"
	unit_label.modulate = TEXT_MUTED
	unit_label.add_theme_font_size_override("font_size", 11)
	dock_header.add_child(unit_label)

	var controls_row := HBoxContainer.new()
	controls_row.add_theme_constant_override("separation", 8)
	dock_box.add_child(controls_row)

	var maneuver_axes := ["PRO", "RAD", "NRM"]
	for axis_index in range(3):
		var axis: String = maneuver_axes[axis_index]
		var column := VBoxContainer.new()
		column.custom_minimum_size.x = 92.0
		var axis_label := Label.new()
		axis_label.text = axis
		axis_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		axis_label.modulate = TEXT_MUTED
		axis_label.add_theme_font_size_override("font_size", 11)
		column.add_child(axis_label)
		var edit := LineEdit.new()
		edit.name = "Impulse%s" % ["X", "Y", "Z"][axis_index]
		edit.text = "0.000"
		edit.alignment = HORIZONTAL_ALIGNMENT_CENTER
		edit.tooltip_text = "顺行 / 径向 / 法向速度增量，单位 AU/年"
		_style_input(edit)
		column.add_child(edit)
		controls_row.add_child(column)
		if axis_index == 0: impulse_x = edit
		elif axis_index == 1: impulse_y = edit
		else: impulse_z = edit

	var time_column := VBoxContainer.new()
	time_column.custom_minimum_size.x = 112.0
	var burn_time_label := Label.new()
	burn_time_label.text = "EPOCH"
	burn_time_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	burn_time_label.modulate = TEXT_MUTED
	burn_time_label.add_theme_font_size_override("font_size", 11)
	time_column.add_child(burn_time_label)
	burn_time = LineEdit.new()
	burn_time.name = "BurnTime"
	burn_time.text = "0.0000"
	burn_time.alignment = HORIZONTAL_ALIGNMENT_CENTER
	burn_time.tooltip_text = "绝对仿真时刻，不能早于当前时间"
	_style_input(burn_time)
	time_column.add_child(burn_time)
	controls_row.add_child(time_column)

	var guidance_button := _make_button("航向辅助", "GuidanceButton", _apply_guidance)
	guidance_button.tooltip_text = "显示目标连线与相对运动信息，不提供答案"
	guidance_button.modulate = Color("b9dce5")
	controls_row.add_child(guidance_button)
	controls_row.add_child(_make_button("预演", "PredictButton", _predict_maneuver))
	var commit_button := _make_button("锁定节点", "CommitButton", _commit_maneuver)
	commit_button.add_theme_color_override("font_color", INK)
	commit_button.add_theme_stylebox_override("normal", _button_style(AMBER, AMBER.lightened(0.18)))
	controls_row.add_child(commit_button)

	var action_row := HBoxContainer.new()
	action_row.add_theme_constant_override("separation", 7)
	dock_box.add_child(action_row)
	time_button = _make_button("▶  推进", "RunButton", _toggle_running)
	time_button.custom_minimum_size.x = 135.0
	time_button.add_theme_stylebox_override("normal", _button_style(Color("173e52"), ACCENT))
	action_row.add_child(time_button)
	action_row.add_child(_make_button("立即点火", "BurnNowButton", _execute_maneuver))
	action_row.add_child(_make_button("取消节点", "CancelButton", _cancel_maneuver))
	action_row.add_child(_make_button("单步", "SingleStepButton", _single_step))
	action_row.add_child(_make_button("− 时间", "SlowerButton", _slower))
	action_row.add_child(_make_button("+ 时间", "FasterButton", _faster))
	action_row.add_child(_make_button("全局 H", "HomeCameraButton", _focus_system))
	action_row.add_child(_make_button("目标 T", "TargetCameraButton", _focus_target))

	flight_hud = PanelContainer.new()
	flight_hud.name = "FlightHUD"
	flight_hud.set_anchors_preset(Control.PRESET_CENTER_BOTTOM)
	flight_hud.offset_left = -410.0
	flight_hud.offset_top = -132.0
	flight_hud.offset_right = 410.0
	flight_hud.offset_bottom = -22.0
	flight_hud.add_theme_stylebox_override("panel", _panel_style(Color(0.014, 0.028, 0.040, 0.90), Color(0.12, 0.48, 0.58, 0.68), 12))
	layer.add_child(flight_hud)
	var flight_row := HBoxContainer.new()
	flight_row.add_theme_constant_override("separation", 24)
	flight_hud.add_child(flight_row)
	var flight_mode_box := VBoxContainer.new()
	flight_mode_box.custom_minimum_size.x = 285.0
	flight_row.add_child(flight_mode_box)
	var flight_mode_title := Label.new()
	flight_mode_title.text = "MANUAL FLIGHT  /  有限推力驾驶"
	flight_mode_title.modulate = ACCENT
	flight_mode_title.add_theme_font_size_override("font_size", 12)
	flight_mode_box.add_child(flight_mode_title)
	flight_status_label = Label.new()
	flight_status_label.name = "FlightStatus"
	flight_status_label.text = "COAST"
	flight_status_label.modulate = Color("d9f7ff")
	flight_status_label.add_theme_font_size_override("font_size", 20)
	flight_mode_box.add_child(flight_status_label)
	flight_controls_label = Label.new()
	flight_controls_label.text = "W/S 顺逆行   A/D 径向   Q/E 法向   Shift 全推力   Ctrl 制动"
	flight_controls_label.modulate = TEXT_MUTED
	flight_controls_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	flight_controls_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	flight_controls_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	flight_controls_label.add_theme_font_size_override("font_size", 13)
	flight_row.add_child(flight_controls_label)

	flight_reticle = Label.new()
	flight_reticle.name = "FlightReticle"
	flight_reticle.text = "＋"
	flight_reticle.set_anchors_preset(Control.PRESET_CENTER)
	flight_reticle.offset_left = -18.0
	flight_reticle.offset_top = -18.0
	flight_reticle.offset_right = 18.0
	flight_reticle.offset_bottom = 18.0
	flight_reticle.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	flight_reticle.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	flight_reticle.modulate = Color(0.36, 0.90, 0.94, 0.62)
	flight_reticle.add_theme_font_size_override("font_size", 22)
	flight_reticle.mouse_filter = Control.MOUSE_FILTER_IGNORE
	layer.add_child(flight_reticle)

	telemetry_toggle = _make_button("专业数据  +", "TelemetryToggle", _toggle_telemetry)
	telemetry_toggle.position = Vector2(0.0, 0.0)
	telemetry_toggle.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	telemetry_toggle.offset_left = -168.0
	telemetry_toggle.offset_top = 94.0
	telemetry_toggle.offset_right = -22.0
	telemetry_toggle.offset_bottom = 132.0
	layer.add_child(telemetry_toggle)

	telemetry_panel = PanelContainer.new()
	telemetry_panel.name = "TelemetryPanel"
	telemetry_panel.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	telemetry_panel.offset_left = -370.0
	telemetry_panel.offset_top = 142.0
	telemetry_panel.offset_right = -22.0
	telemetry_panel.offset_bottom = 352.0
	telemetry_panel.add_theme_stylebox_override("panel", _panel_style(PANEL, Color(0.18, 0.43, 0.55, 0.58), 10))
	telemetry_panel.visible = false
	layer.add_child(telemetry_panel)

	var telemetry_box := VBoxContainer.new()
	telemetry_box.add_theme_constant_override("separation", 8)
	telemetry_panel.add_child(telemetry_box)
	var telemetry_title := Label.new()
	telemetry_title.text = "FLIGHT DYNAMICS / 专业轨道数据"
	telemetry_title.modulate = ACCENT
	telemetry_title.add_theme_font_size_override("font_size", 12)
	telemetry_box.add_child(telemetry_title)

	telemetry_label = Label.new()
	telemetry_label.modulate = Color("a9c1d2")
	telemetry_label.add_theme_font_size_override("font_size", 13)
	telemetry_box.add_child(telemetry_label)

	orbit_label = Label.new()
	orbit_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	orbit_label.modulate = Color("7f9aab")
	orbit_label.add_theme_font_size_override("font_size", 12)
	telemetry_box.add_child(orbit_label)

	var persistence_row := HBoxContainer.new()
	persistence_row.add_child(_make_button("保存", "SaveButton", _save_snapshot))
	persistence_row.add_child(_make_button("读取", "LoadButton", _load_snapshot))
	persistence_row.add_child(_make_button("重开", "RestartButton", _restart_mission))
	telemetry_box.add_child(persistence_row)

	_build_story_overlay(layer)

func _build_story_overlay(layer: CanvasLayer) -> void:
	overlay = ColorRect.new()
	overlay.name = "StoryOverlay"
	overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	overlay.color = Color.BLACK
	overlay.mouse_filter = Control.MOUSE_FILTER_STOP
	layer.add_child(overlay)

	overlay_backdrop = ColorRect.new()
	overlay_backdrop.name = "ProceduralBackdrop"
	overlay_backdrop.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	overlay_backdrop.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var backdrop_material := ShaderMaterial.new()
	backdrop_material.shader = BRIEFING_SHADER
	overlay_backdrop.material = backdrop_material
	overlay.add_child(overlay_backdrop)

	var cinematic_scrim := ColorRect.new()
	cinematic_scrim.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	cinematic_scrim.color = Color(0.005, 0.012, 0.020, 0.18)
	cinematic_scrim.mouse_filter = Control.MOUSE_FILTER_IGNORE
	overlay.add_child(cinematic_scrim)

	var card := PanelContainer.new()
	card.set_anchors_preset(Control.PRESET_CENTER_LEFT)
	card.offset_left = 64.0
	card.offset_top = -250.0
	card.offset_right = 570.0
	card.offset_bottom = 250.0
	card.add_theme_stylebox_override("panel", _panel_style(Color(0.018, 0.032, 0.048, 0.93), Color(0.23, 0.62, 0.72, 0.74), 16))
	overlay.add_child(card)

	var content := VBoxContainer.new()
	content.add_theme_constant_override("separation", 15)
	card.add_child(content)

	var campaign_name := Label.new()
	campaign_name.text = "PHYZBOX  /  %s" % Campaign.CAMPAIGN_TITLE
	campaign_name.add_theme_font_size_override("font_size", 13)
	campaign_name.modulate = ACCENT
	content.add_child(campaign_name)

	overlay_chapter = Label.new()
	overlay_chapter.modulate = TEXT_MUTED
	overlay_chapter.add_theme_font_size_override("font_size", 13)
	content.add_child(overlay_chapter)

	overlay_title = Label.new()
	overlay_title.add_theme_font_size_override("font_size", 36)
	overlay_title.modulate = TEXT_MAIN
	content.add_child(overlay_title)

	var title_rule := HSeparator.new()
	title_rule.modulate = Color(0.25, 0.70, 0.78, 0.55)
	content.add_child(title_rule)

	overlay_story = Label.new()
	overlay_story.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	overlay_story.custom_minimum_size.y = 82
	overlay_story.add_theme_font_size_override("font_size", 16)
	overlay_story.modulate = Color("c5d4df")
	content.add_child(overlay_story)

	var objective_card := PanelContainer.new()
	objective_card.add_theme_stylebox_override("panel", _panel_style(Color(0.04, 0.12, 0.14, 0.86), Color(0.25, 0.78, 0.67, 0.70), 8))
	content.add_child(objective_card)
	overlay_objective = Label.new()
	overlay_objective.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	overlay_objective.modulate = Color("a1f4d5")
	overlay_objective.add_theme_font_size_override("font_size", 16)
	objective_card.add_child(overlay_objective)

	overlay_tutorial = Label.new()
	overlay_tutorial.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	overlay_tutorial.modulate = Color("f2c178")
	overlay_tutorial.add_theme_font_size_override("font_size", 13)
	content.add_child(overlay_tutorial)

	overlay_button = Button.new()
	overlay_button.name = "StoryActionButton"
	overlay_button.custom_minimum_size.y = 54
	overlay_button.add_theme_font_size_override("font_size", 16)
	overlay_button.add_theme_color_override("font_color", INK)
	overlay_button.add_theme_stylebox_override("normal", _button_style(AMBER, AMBER.lightened(0.18)))
	overlay_button.add_theme_stylebox_override("hover", _button_style(AMBER.lightened(0.12), Color.WHITE))
	overlay_button.pressed.connect(_overlay_action_pressed)
	content.add_child(overlay_button)

func _make_button(text: String, name: String, callback: Callable) -> Button:
	var button := Button.new()
	button.name = name
	button.text = text
	button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	button.custom_minimum_size.y = 38.0
	button.add_theme_font_size_override("font_size", 13)
	button.add_theme_color_override("font_color", Color("bed1df"))
	button.add_theme_color_override("font_hover_color", Color.WHITE)
	button.add_theme_stylebox_override("normal", _button_style(Color("102331"), Color("315c70")))
	button.add_theme_stylebox_override("hover", _button_style(Color("17384a"), ACCENT))
	button.add_theme_stylebox_override("pressed", _button_style(Color("0c1a24"), AMBER))
	button.pressed.connect(callback)
	button.add_to_group("playable_control")
	return button

func _panel_style(background: Color, border: Color, radius := 8) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = background
	style.border_color = border
	style.set_border_width_all(1)
	style.set_corner_radius_all(radius)
	style.shadow_color = Color(0.0, 0.0, 0.0, 0.50)
	style.shadow_size = 10
	style.content_margin_left = 14.0
	style.content_margin_right = 14.0
	style.content_margin_top = 12.0
	style.content_margin_bottom = 12.0
	return style

func _button_style(background: Color, border: Color) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = background
	style.border_color = border
	style.set_border_width_all(1)
	style.set_corner_radius_all(6)
	style.content_margin_left = 10.0
	style.content_margin_right = 10.0
	style.content_margin_top = 7.0
	style.content_margin_bottom = 7.0
	return style

func _style_input(edit: LineEdit) -> void:
	edit.custom_minimum_size.y = 38.0
	edit.add_theme_font_size_override("font_size", 15)
	edit.add_theme_color_override("font_color", Color("d8f6ff"))
	edit.add_theme_color_override("caret_color", ACCENT)
	edit.add_theme_stylebox_override("normal", _button_style(Color("081722"), Color("234a5d")))
	edit.add_theme_stylebox_override("focus", _button_style(Color("0b1d29"), ACCENT))

func _make_meter(caption: String, color: Color) -> ProgressBar:
	var row := HBoxContainer.new()
	row.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var label := Label.new()
	label.text = caption
	label.custom_minimum_size.x = 28.0
	label.modulate = TEXT_MUTED
	label.add_theme_font_size_override("font_size", 10)
	row.add_child(label)
	var meter := ProgressBar.new()
	meter.show_percentage = false
	meter.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	meter.custom_minimum_size = Vector2(120.0, 8.0)
	var background := StyleBoxFlat.new()
	background.bg_color = Color("0a1822")
	background.set_corner_radius_all(4)
	var fill := StyleBoxFlat.new()
	fill.bg_color = color
	fill.set_corner_radius_all(4)
	meter.add_theme_stylebox_override("background", background)
	meter.add_theme_stylebox_override("fill", fill)
	row.add_child(meter)
	return meter

func _process(delta: float) -> void:
	if running and not mission_finished:
		var years := delta * 0.03 * float(TIME_RATES[time_rate_index])
		var report: Dictionary
		if view_mode == "flight":
			var thrust_direction := _flight_thrust_direction()
			current_throttle = _flight_throttle(thrust_direction)
			if current_throttle > 0.001:
				last_thrust_direction = thrust_direction
			var physics_thrust := _to_physics(thrust_direction)
			report = simulation.advance_controlled(
				years, physics_thrust.x, physics_thrust.y, physics_thrust.z, current_throttle
			)
			if bool(report.get("fuel_depleted", false)):
				result_label.text = "推进剂预算耗尽，飞船已转入惯性滑行。"
		else:
			current_throttle = 0.0
			report = simulation.advance(years)
		if bool(report.get("burn_executed", false)):
			result_label.text = "机动节点已执行。正在沿新轨迹推进。"
	_update_bodies()
	_update_ship_effects()
	_update_telemetry()
	_update_camera(delta)

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT or event.button_index == MOUSE_BUTTON_RIGHT:
			camera_dragging = event.pressed
		elif event.pressed and event.button_index == MOUSE_BUTTON_WHEEL_UP:
			camera_distance = max(0.28 if view_mode == "flight" else 1.2, camera_distance * 0.86)
		elif event.pressed and event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			camera_distance = min(1.8 if view_mode == "flight" else 24.0, camera_distance * 1.16)
	elif event is InputEventMouseMotion and camera_dragging:
		camera_yaw -= event.relative.x * 0.007
		camera_pitch = clamp(camera_pitch + event.relative.y * 0.006, 0.12, 1.35)
	elif event is InputEventKey and event.pressed and not event.echo:
		match event.physical_keycode:
			KEY_SPACE: _toggle_running()
			KEY_M: _toggle_view_mode()
			KEY_EQUAL, KEY_KP_ADD: _faster()
			KEY_MINUS, KEY_KP_SUBTRACT: _slower()
			KEY_F: _focus_player()
			KEY_T: _focus_target()
			KEY_H: _focus_system()

func _toggle_view_mode() -> void:
	if overlay.visible:
		return
	_set_view_mode("map" if view_mode == "flight" else "flight")

func _set_view_mode(mode: String) -> void:
	view_mode = "map" if mode == "map" else "flight"
	var in_flight := view_mode == "flight"
	command_dock.visible = not in_flight
	flight_hud.visible = in_flight
	flight_reticle.visible = in_flight
	navigation_grid.visible = not in_flight
	prediction_mesh.visible = not in_flight
	telemetry_toggle.visible = not in_flight
	if in_flight:
		telemetry_panel.visible = false
		telemetry_toggle.text = "专业数据  +"
		view_toggle.text = "M  星图规划"
		camera_focus_body_id = selected_body_id
		camera_distance = 0.52
		camera_yaw = 0.0
		camera_pitch = 0.24
		running = not mission_finished
	else:
		view_toggle.text = "M  返回驾驶"
		camera_focus_body_id = primary_body_id
		_fit_camera_to_system()
		running = false
	if time_button:
		time_button.text = "Ⅱ  暂停" if running else "▶  推进"
	target_line_mesh.visible = guidance_enabled or not in_flight
	result_label.text = "直接驾驶：有限推力与燃料实时进入物理积分。" if in_flight else "星图暂停：规划机动节点后按 M 返回驾驶。"

func _flight_thrust_direction() -> Vector3:
	var frame := _orbital_frame()
	if frame.is_empty():
		return Vector3.ZERO
	var prograde: Vector3 = frame[0]
	var radial: Vector3 = frame[1]
	var normal: Vector3 = frame[2]
	if Input.is_physical_key_pressed(KEY_CTRL):
		return -prograde
	var command := Vector3.ZERO
	if Input.is_physical_key_pressed(KEY_W): command += prograde
	if Input.is_physical_key_pressed(KEY_S): command -= prograde
	if Input.is_physical_key_pressed(KEY_D): command += radial
	if Input.is_physical_key_pressed(KEY_A): command -= radial
	if Input.is_physical_key_pressed(KEY_E): command += normal
	if Input.is_physical_key_pressed(KEY_Q): command -= normal
	return command.normalized() if command.length_squared() > 1.0e-8 else Vector3.ZERO

func _flight_throttle(direction: Vector3) -> float:
	if direction.is_zero_approx():
		return 0.0
	return 1.0 if Input.is_physical_key_pressed(KEY_SHIFT) else 0.42

func _orbital_frame() -> Array:
	if not body_local_positions.has(selected_body_id) or not body_local_velocities.has(selected_body_id):
		return []
	var radial: Vector3 = Vector3(body_local_positions[selected_body_id]).normalized()
	var prograde: Vector3 = Vector3(body_local_velocities[selected_body_id]).normalized()
	if radial.is_zero_approx() or prograde.is_zero_approx():
		return []
	var normal := radial.cross(prograde).normalized()
	if normal.is_zero_approx():
		return []
	prograde = normal.cross(radial).normalized()
	return [prograde, radial, normal]

func _orbital_up_for(body_id: int) -> Vector3:
	if body_local_positions.has(body_id):
		var radial := Vector3(body_local_positions[body_id]).normalized()
		if not radial.is_zero_approx():
			return radial
	return Vector3.UP

func _to_physics(world_vector: Vector3) -> Vector3:
	return Vector3(world_vector.x, -world_vector.z, world_vector.y)

func _maneuver_delta_world() -> Vector3:
	var frame := _orbital_frame()
	if frame.is_empty():
		return Vector3.ZERO
	var local_delta := Vector3(_number(impulse_x), _number(impulse_y), _number(impulse_z))
	return Vector3(frame[0]) * local_delta.x + Vector3(frame[1]) * local_delta.y + Vector3(frame[2]) * local_delta.z

func _inertial_world_to_orbital(world_delta: Vector3) -> Vector3:
	var frame := _orbital_frame()
	if frame.is_empty():
		return Vector3.ZERO
	return Vector3(
		world_delta.dot(Vector3(frame[0])),
		world_delta.dot(Vector3(frame[1])),
		world_delta.dot(Vector3(frame[2]))
	)

func _mission_selected(index: int) -> void:
	if index > unlocked_mission:
		mission_picker.select(current_mission)
		result_label.text = "该任务尚未解锁。完成当前航线后继续。"
		return
	_load_mission(index)

func _load_mission(index: int, show_briefing := true) -> void:
	current_mission = clampi(index, 0, Campaign.MISSIONS.size() - 1)
	time_rate_index = 1
	running = false
	mission_finished = false
	if time_button:
		time_button.text = "▶  推进"
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
	_predict_maneuver(false)
	_set_view_mode("flight")
	result_label.text = "按 W/S 沿轨道顺逆行推进；按 M 进入星图规划。"
	if show_briefing:
		running = false
		time_button.text = "▶  推进"
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
	var chapter_index := int(mission.chapter)
	var accents := [Color("38c8dc"), Color("7d8cff"), Color("ef9a4a")]
	var backdrop_material := overlay_backdrop.material as ShaderMaterial
	backdrop_material.set_shader_parameter("chapter", float(chapter_index))
	backdrop_material.set_shader_parameter("accent_color", accents[chapter_index])
	overlay_chapter.text = str(chapter.title)
	overlay_title.text = "%s // %s" % [mission.code, mission.title]
	overlay_story.text = str(mission.story)
	overlay_objective.text = "MISSION  /  %s" % mission.objective
	overlay_tutorial.text = "NAV  /  %s" % mission.tutorial
	overlay_button.text = "进入航线   →"
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
		overlay_button.text = "进入下一任务   →"
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
			_set_view_mode("flight")
			result_label.text = "手动驾驶已接管。W/S 顺逆行，A/D 径向，Q/E 法向，M 打开星图。"

func _apply_guidance() -> void:
	guidance_enabled = not guidance_enabled
	target_line_mesh.visible = guidance_enabled or view_mode == "map"
	result_label.text = "航向辅助已开启：只显示目标方向与相对运动，不提供机动答案。" if guidance_enabled else "航向辅助已关闭。"

func _clear_visuals() -> void:
	for node in body_nodes.values():
		node.queue_free()
	for node in trail_meshes.values():
		node.queue_free()
	body_nodes.clear()
	body_visuals.clear()
	body_local_positions.clear()
	body_local_velocities.clear()
	trail_meshes.clear()
	trails.clear()
	prediction_mesh.mesh = null
	target_line_mesh.mesh = null

func _update_bodies() -> void:
	var bodies: Array = simulation.get_bodies()
	var origin := Vector3.ZERO
	var origin_velocity := Vector3.ZERO
	for data in bodies:
		if int(data.id) == primary_body_id:
			origin = _v3(data.position)
			origin_velocity = _v3(data.velocity)
	for data in bodies:
		var id := int(data.id)
		if not body_nodes.has(id):
			_create_body_node(data)
		var local_position := _v3(data.position) - origin
		var local_velocity := _v3(data.velocity) - origin_velocity
		body_local_positions[id] = local_position
		body_local_velocities[id] = local_velocity
		body_nodes[id].position = local_position
		if body_visuals.has(id) and int(data.kind) == 6:
			var facing_direction := local_velocity.normalized()
			if id == selected_body_id and current_throttle > 0.001 and not last_thrust_direction.is_zero_approx():
				facing_direction = last_thrust_direction
			if facing_direction.length_squared() > 0.001:
				var visual: Node3D = body_visuals[id]
				visual.look_at(visual.global_position + facing_direction, _orbital_up_for(id))
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

	var color: Color = COLORS[int(data.kind) % COLORS.size()]
	if id == selected_body_id: color = PLAYER_COLOR
	elif id == target_body_id: color = TARGET_COLOR
	elif id == primary_body_id: color = PRIMARY_COLOR

	var visual: Node3D
	if int(data.kind) == 6:
		visual = _create_spacecraft_visual(radius, id == selected_body_id, id == target_body_id)
	elif str(data.name).contains("Station"):
		visual = _create_station_visual(radius)
	else:
		visual = _create_celestial_visual(
			radius, color, id == primary_body_id, int(data.kind), str(data.name), id
		)
	visual.name = "SpacecraftVisual" if int(data.kind) == 6 else "BodyVisual"
	holder.add_child(visual)
	body_visuals[id] = visual

	var role := str(data.name).to_upper()
	if id == selected_body_id and int(data.kind) == 6:
		role = "%s  /  你的飞船" % role
	elif id == target_body_id:
		role = "◇  %s" % role
	var label := Label3D.new()
	label.text = role
	label.position = Vector3(0.0, radius + 0.10, 0.0)
	label.font_size = 21 if id == primary_body_id else 24
	label.outline_size = 6
	label.pixel_size = 0.0024
	label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	label.no_depth_test = true
	label.modulate = color.lightened(0.10)
	holder.add_child(label)

	if id == selected_body_id or id == target_body_id:
		var marker_color := PLAYER_COLOR if id == selected_body_id else TARGET_COLOR
		holder.add_child(_make_marker(radius + 0.075, marker_color))

	var trail := MeshInstance3D.new()
	trail.name = "Trail_%s" % id
	add_child(trail)
	trail_meshes[id] = trail

func _create_spacecraft_visual(radius: float, is_player: bool, is_target: bool) -> Node3D:
	var root := Node3D.new()
	root.scale = Vector3.ONE * (radius / 0.085)

	var hull := _space_material(Color("7f8d96"), Color("243943"), 0.15)
	var dark_hull := _space_material(Color("202b32"), Color("0b151b"), 0.10)
	var ceramic := _space_material(Color("d7d8d1"), Color("626968"), 0.10)
	var solar := _space_material(Color("0a3557"), Color("1475a6"), 0.48)
	var cyan_light := _space_material(Color("66e9ff"), Color("42dfff"), 2.2)
	var amber_light := _space_material(Color("ffb34f"), Color("ff9a35"), 1.8)

	_add_cylinder(root, 0.043, 0.043, 0.22, Vector3.ZERO, Vector3(PI * 0.5, 0.0, 0.0), hull)
	_add_cylinder(root, 0.052, 0.045, 0.055, Vector3(0.0, 0.0, -0.13), Vector3(PI * 0.5, 0.0, 0.0), ceramic)
	_add_cylinder(root, 0.050, 0.038, 0.050, Vector3(0.0, 0.0, 0.13), Vector3(PI * 0.5, 0.0, 0.0), dark_hull)

	_add_box(root, Vector3(0.16, 0.010, 0.018), Vector3(-0.10, 0.0, 0.0), dark_hull)
	_add_box(root, Vector3(0.16, 0.010, 0.018), Vector3(0.10, 0.0, 0.0), dark_hull)
	for side in [-1.0, 1.0]:
		var panel_x: float = float(side) * 0.19
		_add_box(root, Vector3(0.20, 0.008, 0.082), Vector3(panel_x, 0.0, 0.0), solar)
		_add_box(root, Vector3(0.003, 0.010, 0.080), Vector3(panel_x, 0.0, 0.0), dark_hull)
		_add_box(root, Vector3(0.196, 0.010, 0.003), Vector3(panel_x, 0.0, 0.0), dark_hull)

	_add_box(root, Vector3(0.075, 0.006, 0.090), Vector3(0.0, 0.072, 0.012), ceramic)
	_add_box(root, Vector3(0.075, 0.006, 0.090), Vector3(0.0, -0.072, 0.012), ceramic)
	_add_cylinder(root, 0.004, 0.004, 0.080, Vector3(0.0, 0.082, -0.035), Vector3.ZERO, dark_hull)
	_add_cylinder(root, 0.030, 0.030, 0.006, Vector3(0.0, 0.122, -0.035), Vector3.ZERO, ceramic)

	for x_sign in [-1.0, 1.0]:
		for y_sign in [-1.0, 1.0]:
			var nozzle_position := Vector3(float(x_sign) * 0.022, float(y_sign) * 0.020, 0.165)
			_add_cylinder(root, 0.011, 0.016, 0.040, nozzle_position, Vector3(PI * 0.5, 0.0, 0.0), dark_hull)
			var plume := _add_cylinder(
				root, 0.001, 0.010, 0.060,
				nozzle_position + Vector3(0.0, 0.0, 0.047),
				Vector3(PI * 0.5, 0.0, 0.0), cyan_light
			)
			plume.name = "DrivePlume"
			plume.visible = false
			if is_player:
				plume.add_to_group("player_drive_plume")

	for pod_x in [-0.052, 0.052]:
		_add_box(root, Vector3(0.022, 0.022, 0.040), Vector3(pod_x, 0.0, -0.070), dark_hull)

	var status_material := cyan_light if is_player else (amber_light if is_target else cyan_light)
	_add_sphere(root, 0.009, Vector3(0.0, 0.052, -0.125), status_material)
	if is_player:
		var drive_light := OmniLight3D.new()
		drive_light.name = "DriveLight"
		drive_light.position = Vector3(0.0, 0.0, 0.22)
		drive_light.light_color = Color("50dcff")
		drive_light.omni_range = 0.85
		drive_light.light_energy = 0.0
		drive_light.visible = false
		drive_light.add_to_group("player_drive_light")
		root.add_child(drive_light)
	return root

func _create_station_visual(radius: float) -> Node3D:
	var root := Node3D.new()
	root.scale = Vector3.ONE * (radius / 0.10)
	var metal := _space_material(Color("77838d"), Color("2b3941"), 0.20)
	var solar := _space_material(Color("102443"), Color("6f2471"), 0.45)
	var signal_material := _space_material(TARGET_COLOR, TARGET_COLOR, 1.7)
	var torus := TorusMesh.new()
	torus.inner_radius = 0.055
	torus.outer_radius = 0.072
	torus.rings = 18
	torus.ring_segments = 32
	var ring := MeshInstance3D.new()
	ring.mesh = torus
	ring.rotation_degrees = Vector3(68.0, 18.0, 0.0)
	ring.material_override = metal
	root.add_child(ring)
	_add_cylinder(root, 0.026, 0.026, 0.120, Vector3.ZERO, Vector3(PI * 0.5, 0.0, 0.0), metal)
	_add_box(root, Vector3(0.20, 0.006, 0.050), Vector3.ZERO, solar)
	_add_sphere(root, 0.014, Vector3(0.0, 0.050, 0.0), signal_material)
	return root

func _create_celestial_visual(
	radius: float, color: Color, is_primary: bool, kind: int, body_name: String, body_id: int
) -> Node3D:
	var root := Node3D.new()
	var sphere := SphereMesh.new()
	sphere.radius = radius
	sphere.height = radius * 2.0
	sphere.radial_segments = 36
	sphere.rings = 20
	var material: Material
	if kind == 1:
		var planet_material := ShaderMaterial.new()
		planet_material.shader = PLANET_SHADER
		planet_material.set_shader_parameter("land_color", color.darkened(0.28))
		planet_material.set_shader_parameter("highland_color", color.lightened(0.18))
		planet_material.set_shader_parameter("ocean_color", Color("071c2d").lerp(color.darkened(0.5), 0.35))
		planet_material.set_shader_parameter("ice_color", Color("d8ecf2"))
		planet_material.set_shader_parameter("seed", float(body_id) * 1.713)
		planet_material.set_shader_parameter("gas_giant", 1.0 if body_name == "Giant" else 0.0)
		material = planet_material
	else:
		material = _space_material(color.darkened(0.10), color, 1.45 if is_primary else 0.28)
	var body := MeshInstance3D.new()
	body.mesh = sphere
	body.material_override = material
	if kind == 5:
		body.scale = Vector3(1.18, 0.82, 0.96)
	root.add_child(body)
	if kind == 1:
		var atmosphere_mesh := SphereMesh.new()
		atmosphere_mesh.radius = radius * 1.055
		atmosphere_mesh.height = radius * 2.11
		atmosphere_mesh.radial_segments = 32
		atmosphere_mesh.rings = 18
		var atmosphere := MeshInstance3D.new()
		atmosphere.name = "ProceduralAtmosphere"
		atmosphere.mesh = atmosphere_mesh
		var atmosphere_material := StandardMaterial3D.new()
		atmosphere_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		atmosphere_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		atmosphere_material.cull_mode = BaseMaterial3D.CULL_FRONT
		atmosphere_material.albedo_color = Color(color.r, color.g, color.b, 0.13)
		atmosphere_material.emission_enabled = true
		atmosphere_material.emission = color * 0.22
		atmosphere.material_override = atmosphere_material
		root.add_child(atmosphere)
		if body_name == "Cinder":
			var ring_mesh := TorusMesh.new()
			ring_mesh.inner_radius = radius * 1.38
			ring_mesh.outer_radius = radius * 1.46
			ring_mesh.rings = 10
			ring_mesh.ring_segments = 64
			var planet_ring := MeshInstance3D.new()
			planet_ring.name = "CinderRing"
			planet_ring.mesh = ring_mesh
			planet_ring.rotation_degrees = Vector3(67.0, 8.0, 0.0)
			var ring_material := StandardMaterial3D.new()
			ring_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
			ring_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
			ring_material.albedo_color = Color(0.58, 0.72, 0.77, 0.32)
			planet_ring.material_override = ring_material
			root.add_child(planet_ring)
	if is_primary:
		var halo_sphere := SphereMesh.new()
		halo_sphere.radius = radius * 1.18
		halo_sphere.height = radius * 2.36
		halo_sphere.radial_segments = 28
		halo_sphere.rings = 14
		var halo := MeshInstance3D.new()
		halo.mesh = halo_sphere
		var halo_material := _space_material(Color(color.r, color.g, color.b, 0.10), color, 0.9)
		halo_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		halo_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		halo.material_override = halo_material
		root.add_child(halo)
	return root

func _space_material(albedo: Color, emission_color: Color, emission_strength: float) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = albedo
	if emission_strength > 0.0:
		material.emission_enabled = true
		material.emission = emission_color * emission_strength
	material.metallic = 0.55
	material.roughness = 0.42
	return material

func _add_box(parent: Node3D, size: Vector3, position: Vector3, material: Material) -> MeshInstance3D:
	var mesh := BoxMesh.new()
	mesh.size = size
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.position = position
	instance.material_override = material
	parent.add_child(instance)
	return instance

func _add_cylinder(parent: Node3D, top_radius: float, bottom_radius: float, height: float,
		position: Vector3, rotation: Vector3, material: Material) -> MeshInstance3D:
	var mesh := CylinderMesh.new()
	mesh.top_radius = top_radius
	mesh.bottom_radius = bottom_radius
	mesh.height = height
	mesh.radial_segments = 18
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.position = position
	instance.rotation = rotation
	instance.material_override = material
	parent.add_child(instance)
	return instance

func _add_sphere(parent: Node3D, radius: float, position: Vector3, material: Material) -> MeshInstance3D:
	var mesh := SphereMesh.new()
	mesh.radius = radius
	mesh.height = radius * 2.0
	mesh.radial_segments = 18
	mesh.rings = 10
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.position = position
	instance.material_override = material
	parent.add_child(instance)
	return instance

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
	var physics_delta := _to_physics(_maneuver_delta_world())
	var prediction: Array = simulation.predict(
		selected_body_id,
		physics_delta.x, physics_delta.y, physics_delta.z,
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
	var physics_delta := _to_physics(_maneuver_delta_world())
	var accepted: bool = simulation.apply_impulse(
		selected_body_id, physics_delta.x, physics_delta.y, physics_delta.z
	)
	result_label.text = "立即点火完成。" if accepted else "点火被拒绝：燃料不足、数值无效或已有节点占用预算。"
	if accepted:
		_predict_maneuver(false)

func _commit_maneuver() -> void:
	var physics_delta := _to_physics(_maneuver_delta_world())
	var accepted: bool = simulation.schedule_impulse(
		selected_body_id, _number(burn_time),
		physics_delta.x, physics_delta.y, physics_delta.z
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
	var deadline := float(definition.deadline)
	var budget := float(definition.delta_v_budget)
	time_bar.value = clampf(simulation.get_time() / maxf(deadline, 1.0e-9) * 100.0, 0.0, 100.0)
	fuel_bar.value = clampf((budget - float(definition.delta_v_spent) - reserved) / maxf(budget, 1.0e-9) * 100.0, 0.0, 100.0)
	telemetry_label.text = "时间 %.4f / %.2f年   倍速 %.2fx\n燃料 %.3f已用 + %.3f预留 / %.3f AU/年   节点 %s\n%s · 固定步长 %s年" % [
		simulation.get_time(), deadline, float(TIME_RATES[time_rate_index]),
		float(definition.delta_v_spent), reserved, budget, node_status,
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
			progress_label.text = "RANGE  %.3f AU     GOAL  < 0.120" % float(evaluation.distance)
		1:
			progress_label.text = "RANGE %.3f AU    REL-V %.3f AU/yr" % [
				float(evaluation.distance), float(evaluation.relative_speed)
			]
		2:
			progress_label.text = "EXIT ENERGY  %.1f%%     GOAL  > 96%%" % (float(evaluation.speed_ratio) * 100.0)
		3:
			progress_label.text = "MISS DISTANCE  %.3f AU     GOAL  > 0.250" % float(evaluation.distance)
		4:
			progress_label.text = "SURVIVAL  %.3f / 1.400 yr" % simulation.get_time()

func _complete_mission(score: float) -> void:
	mission_finished = true
	running = false
	time_button.text = "▶  推进"
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
	time_button.text = "▶  推进"
	result_label.text = "任务失败。读取快照或重新开始。"
	_show_debrief(false)

func _toggle_running() -> void:
	if overlay.visible or mission_finished:
		return
	running = not running
	time_button.text = "Ⅱ  暂停" if running else "▶  推进"

func _toggle_telemetry() -> void:
	telemetry_panel.visible = not telemetry_panel.visible
	telemetry_toggle.text = "专业数据  −" if telemetry_panel.visible else "专业数据  +"

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
			_set_impulse(_inertial_world_to_orbital(_v3(delta_values)))
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
	if view_mode == "flight":
		var frame := _orbital_frame()
		if not frame.is_empty():
			var prograde := Vector3(frame[0])
			var radial := Vector3(frame[1])
			var normal := Vector3(frame[2])
			var right := normal.cross(prograde).normalized()
			var horizontal := cos(camera_pitch) * camera_distance
			var back := (-prograde * cos(camera_yaw) + right * sin(camera_yaw)).normalized()
			camera.position = camera_focus + back * horizontal + radial * sin(camera_pitch) * camera_distance
			camera.look_at(camera_focus + prograde * 0.13, radial)
			return
	var horizontal := cos(camera_pitch) * camera_distance
	var offset := Vector3(
		sin(camera_yaw) * horizontal,
		sin(camera_pitch) * camera_distance,
		cos(camera_yaw) * horizontal
	)
	camera.position = camera_focus + offset
	camera.look_at(camera_focus, Vector3.UP)

func _update_ship_effects() -> void:
	var burning := current_throttle > 0.001 and view_mode == "flight" and running
	var pulse := 0.92 + 0.08 * sin(float(Time.get_ticks_msec()) * 0.026)
	for plume in get_tree().get_nodes_in_group("player_drive_plume"):
		if plume is MeshInstance3D:
			plume.visible = burning
			plume.scale = Vector3(1.0, (0.45 + current_throttle * 1.75) * pulse, 1.0)
	for drive_light in get_tree().get_nodes_in_group("player_drive_light"):
		if drive_light is OmniLight3D:
			drive_light.visible = burning
			drive_light.light_energy = current_throttle * (2.8 + pulse)
	if not flight_status_label:
		return
	var definition: Dictionary = simulation.get_mission_definition()
	var evaluation: Dictionary = simulation.evaluate_mission()
	var budget := float(definition.delta_v_budget)
	var remaining := maxf(0.0, budget - float(definition.delta_v_spent) - float(definition.get("delta_v_reserved", 0.0)))
	var speed := Vector3(body_local_velocities.get(selected_body_id, Vector3.ZERO)).length()
	flight_status_label.text = "%s  %3d%%     V %.3f     ΔV %.3f" % [
		("BURN" if burning else "COAST"), roundi(current_throttle * 100.0), speed, remaining
	]
	flight_status_label.modulate = AMBER if burning else Color("d9f7ff")
	flight_controls_label.text = "RANGE %.3f AU   REL-V %.3f   ·   W/S A/D Q/E   SHIFT 全推力   M 星图" % [
		float(evaluation.distance), float(evaluation.relative_speed)
	]

func _set_impulse(value: Vector3) -> void:
	impulse_x.text = "%.3f" % value.x
	impulse_y.text = "%.3f" % value.y
	impulse_z.text = "%.3f" % value.z

func _number(edit: LineEdit) -> float:
	return edit.text.to_float() if edit else 0.0

func _v3(values) -> Vector3:
	return Vector3(float(values[0]), float(values[2]), -float(values[1]))
