shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, fog_disabled;

varying flat vec3 star_color;
varying flat float star_intensity;
uniform float display_gain : hint_range(0.0, 4.0) = 1.0;

void vertex() {
	float size = INSTANCE_CUSTOM.r;
	star_intensity = INSTANCE_CUSTOM.g;
	star_color = COLOR.rgb;
	MODELVIEW_MATRIX = VIEW_MATRIX * mat4(
		INV_VIEW_MATRIX[0],
		INV_VIEW_MATRIX[1],
		INV_VIEW_MATRIX[2],
		MODEL_MATRIX[3]
	);
	VERTEX.xy *= size;
}

void fragment() {
	vec2 centered = (UV - vec2(0.5)) * 2.0;
	float radius = length(centered);
	float halo = smoothstep(1.0, 0.18, radius);
	float core = smoothstep(0.28, 0.0, radius);
	float energy = (halo * 0.42 + core * 1.35) * star_intensity * display_gain;
	ALBEDO = star_color * energy;
	EMISSION = star_color * energy;
	ALPHA = smoothstep(1.0, 0.12, radius) * clamp(star_intensity, 0.18, 1.0);
}
