extends RefCounted

const MAGIC := "PHYZEPH2"
const AU_KM := 149_597_870.7

var body_ids: Array[int] = []
var body_names: Dictionary = {}
var body_indices: Dictionary = {}
var epochs := PackedFloat64Array()
var utc_millis := PackedInt64Array()
var all_states := PackedFloat64Array()
var loaded := false
var load_error := ""

func load_file(path: String) -> bool:
	loaded = false
	load_error = ""
	var file := FileAccess.open(path, FileAccess.READ)
	if not file:
		load_error = "无法打开历史星历：%s" % path
		return false
	var magic := file.get_buffer(8).get_string_from_ascii()
	if magic != MAGIC:
		load_error = "星历格式不匹配：%s" % magic
		return false
	var version := file.get_32()
	var body_count := file.get_32()
	var sample_count := file.get_32()
	if version != 2 or body_count == 0 or sample_count < 2:
		load_error = "星历头无效"
		return false

	body_ids.clear()
	body_names.clear()
	body_indices.clear()
	for body_index in range(body_count):
		var unsigned_id := file.get_32()
		var body_id := int(unsigned_id if unsigned_id < 0x80000000 else unsigned_id - 0x100000000)
		var name_length := file.get_16()
		var body_name := file.get_buffer(name_length).get_string_from_utf8()
		body_ids.append(body_id)
		body_names[body_id] = body_name
		body_indices[body_id] = body_index

	epochs.resize(sample_count)
	utc_millis.resize(sample_count)
	all_states.resize(sample_count * body_count * 6)
	for sample_index in range(sample_count):
		epochs[sample_index] = file.get_double()
		utc_millis[sample_index] = int(file.get_64())
		for body_index in range(body_count):
			var offset := (sample_index * body_count + body_index) * 6
			for component in range(6):
				all_states[offset + component] = file.get_double()
	loaded = file.get_position() == file.get_length()
	if not loaded:
		load_error = "星历数据长度不完整"
	return loaded

func start_epoch() -> float:
	return epochs[0] if not epochs.is_empty() else 0.0

func end_epoch() -> float:
	return epochs[epochs.size() - 1] if not epochs.is_empty() else 0.0

func epoch_for_utc(unix_seconds: float) -> float:
	if epochs.is_empty():
		return 0.0
	var target := roundi(unix_seconds * 1000.0)
	var pair := _utc_pair(target)
	var lower := int(pair.x)
	var upper := int(pair.y)
	if lower == upper:
		return epochs[lower]
	var span := float(utc_millis[upper] - utc_millis[lower])
	var alpha := clampf(float(target - utc_millis[lower]) / maxf(span, 1.0), 0.0, 1.0)
	return lerpf(epochs[lower], epochs[upper], alpha)

func unix_time_for_epoch(epoch: float) -> float:
	if epochs.is_empty():
		return 0.0
	var pair := _epoch_pair(epoch)
	var lower := int(pair.x)
	var upper := int(pair.y)
	if lower == upper:
		return float(utc_millis[lower]) / 1000.0
	var alpha := clampf((epoch - epochs[lower]) / maxf(epochs[upper] - epochs[lower], 1.0e-9), 0.0, 1.0)
	return lerpf(float(utc_millis[lower]), float(utc_millis[upper]), alpha) / 1000.0

func state(body_id: int, epoch: float) -> Dictionary:
	var components := _interpolate(body_id, epoch)
	if components.is_empty():
		return {"valid": false}
	return {
		"valid": true,
		"position": Vector3(components[0], components[1], components[2]),
		"velocity": Vector3(components[3], components[4], components[5]),
	}

func relative_state(body_id: int, observer_id: int, epoch: float, position_scale := 1.0) -> Dictionary:
	var target := _interpolate(body_id, epoch)
	if target.is_empty():
		return {"valid": false}
	var observer := PackedFloat64Array([0.0, 0.0, 0.0, 0.0, 0.0, 0.0])
	if observer_id != 0:
		observer = _interpolate(observer_id, epoch)
		if observer.is_empty():
			return {"valid": false}
	return {
		"valid": true,
		"position": Vector3(
			(target[0] - observer[0]) * position_scale,
			(target[2] - observer[2]) * position_scale,
			-(target[1] - observer[1]) * position_scale
		),
		"velocity": Vector3(
			target[3] - observer[3],
			target[5] - observer[5],
			-(target[4] - observer[4])
		),
	}

func track(body_id: int, observer_id: int, start: float, stop: float, max_points := 1800) -> PackedVector3Array:
	var result := PackedVector3Array()
	if not body_indices.has(body_id) or epochs.is_empty():
		return result
	var start_index := int(_epoch_pair(start).y)
	var stop_index := int(_epoch_pair(stop).x)
	var count := maxi(0, stop_index - start_index + 1)
	var stride := maxi(1, ceili(float(count) / float(maxi(1, max_points))))
	for index in range(start_index, stop_index + 1, stride):
		var relative := relative_state(body_id, observer_id, epochs[index], 1.0 / AU_KM)
		if bool(relative.valid):
			result.append(relative.position)
	return result

func _interpolate(body_id: int, epoch: float) -> PackedFloat64Array:
	if not loaded or not body_indices.has(body_id):
		return PackedFloat64Array()
	var pair := _epoch_pair(epoch)
	var lower := int(pair.x)
	var upper := int(pair.y)
	var body_index := int(body_indices[body_id])
	var body_count := body_ids.size()
	var lower_offset := (lower * body_count + body_index) * 6
	var upper_offset := (upper * body_count + body_index) * 6
	for component in range(6):
		if not is_finite(all_states[lower_offset + component]) or not is_finite(all_states[upper_offset + component]):
			return PackedFloat64Array()
	var result := PackedFloat64Array()
	result.resize(6)
	if lower == upper:
		for component in range(6):
			result[component] = all_states[lower_offset + component]
		return result

	var interval := epochs[upper] - epochs[lower]
	var t := clampf((epoch - epochs[lower]) / interval, 0.0, 1.0)
	var t2 := t * t
	var t3 := t2 * t
	var h00 := 2.0 * t3 - 3.0 * t2 + 1.0
	var h10 := t3 - 2.0 * t2 + t
	var h01 := -2.0 * t3 + 3.0 * t2
	var h11 := t3 - t2
	for axis in range(3):
		var p0 := all_states[lower_offset + axis]
		var p1 := all_states[upper_offset + axis]
		var v0 := all_states[lower_offset + 3 + axis]
		var v1 := all_states[upper_offset + 3 + axis]
		result[axis] = h00 * p0 + h10 * interval * v0 + h01 * p1 + h11 * interval * v1
		result[3 + axis] = (
			(6.0 * t2 - 6.0 * t) * p0 / interval
			+ (3.0 * t2 - 4.0 * t + 1.0) * v0
			+ (-6.0 * t2 + 6.0 * t) * p1 / interval
			+ (3.0 * t2 - 2.0 * t) * v1
		)
	return result

func _epoch_pair(epoch: float) -> Vector2i:
	if epoch <= epochs[0]:
		return Vector2i.ZERO
	var last := epochs.size() - 1
	if epoch >= epochs[last]:
		return Vector2i(last, last)
	var lower := 0
	var upper := last
	while upper - lower > 1:
		var middle := (lower + upper) / 2
		if epochs[middle] <= epoch:
			lower = middle
		else:
			upper = middle
	if abs(epoch - epochs[lower]) < 1.0e-6:
		return Vector2i(lower, lower)
	if abs(epoch - epochs[upper]) < 1.0e-6:
		return Vector2i(upper, upper)
	return Vector2i(lower, upper)

func _utc_pair(value: int) -> Vector2i:
	if value <= utc_millis[0]:
		return Vector2i.ZERO
	var last := utc_millis.size() - 1
	if value >= utc_millis[last]:
		return Vector2i(last, last)
	var lower := 0
	var upper := last
	while upper - lower > 1:
		var middle := (lower + upper) / 2
		if utc_millis[middle] <= value:
			lower = middle
		else:
			upper = middle
	return Vector2i(lower, upper)
