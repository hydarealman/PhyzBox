shader_type spatial;
render_mode unshaded, blend_mix, depth_draw_never, cull_disabled;

float band(float radial, float start, float end, float softness) {
	return smoothstep(start, start + softness, radial) * (1.0 - smoothstep(end - softness, end, radial));
}

void fragment() {
	float radial = UV.x;
	float c_ring = band(radial, 0.08, 0.33, 0.018) * 0.22;
	float b_ring = band(radial, 0.35, 0.67, 0.012) * 0.72;
	float cassini_gap = band(radial, 0.67, 0.725, 0.008);
	float a_ring = band(radial, 0.725, 0.93, 0.012) * 0.48;
	float f_ring = band(radial, 0.965, 0.995, 0.006) * 0.20;
	float opacity = max(max(c_ring, b_ring), max(a_ring, f_ring));
	opacity *= 1.0 - cassini_gap * 0.96;
	float fine_bands = sin(radial * 680.0) * 0.5 + 0.5;
	float broad_bands = sin(radial * 83.0 + fine_bands * 0.7) * 0.5 + 0.5;
	vec3 warm_ice = mix(vec3(0.50, 0.46, 0.38), vec3(0.91, 0.84, 0.68), broad_bands);
	warm_ice *= 0.82 + fine_bands * 0.18;
	ALBEDO = warm_ice;
	EMISSION = warm_ice * 0.055;
	ALPHA = opacity * (0.82 + fine_bands * 0.18);
}
