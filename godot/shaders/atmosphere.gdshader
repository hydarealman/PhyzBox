shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_front;

uniform vec4 atmosphere_color : source_color = vec4(0.20, 0.55, 0.95, 1.0);
uniform float intensity : hint_range(0.0, 2.0) = 0.8;

void fragment() {
	float rim = pow(clamp(1.0 - abs(dot(normalize(NORMAL), normalize(VIEW))), 0.0, 1.0), 3.2);
	vec3 glow = atmosphere_color.rgb * rim * intensity;
	ALBEDO = glow;
	EMISSION = glow;
	ALPHA = rim * clamp(intensity, 0.0, 1.0) * 0.72;
}
