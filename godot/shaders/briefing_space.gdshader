shader_type canvas_item;
render_mode unshaded;

uniform float chapter = 0.0;
uniform vec4 accent_color : source_color = vec4(0.20, 0.75, 0.86, 1.0);

float hash21(vec2 p) {
	p = fract(p * vec2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}

vec2 hash22(vec2 p) {
	return vec2(hash21(p + 17.17), hash21(p + 83.41));
}

float ring_line(vec2 uv, vec2 center, vec2 scale, float radius, float width) {
	float d = abs(length((uv - center) * scale) - radius);
	return 1.0 - smoothstep(width, width * 2.4, d);
}

void fragment() {
	vec2 uv = UV;
	vec3 background = mix(vec3(0.006, 0.013, 0.022), vec3(0.012, 0.031, 0.044), 1.0 - uv.y);
	float vignette = smoothstep(0.92, 0.18, length((uv - vec2(0.5)) * vec2(1.15, 0.90)));
	background *= 0.55 + 0.45 * vignette;

	vec2 star_grid = vec2(210.0, 118.0);
	vec2 cell = floor(uv * star_grid);
	vec2 local = fract(uv * star_grid);
	float seed = hash21(cell + chapter * 31.7);
	vec2 star_position = hash22(cell + chapter * 13.1);
	float star = (1.0 - smoothstep(0.0, 0.045, length(local - star_position))) * step(0.982, seed);
	float bright_star = step(0.9965, seed);
	vec3 color = background + vec3(star * (0.30 + bright_star * 1.25));

	vec2 planet_center = vec2(0.82, 0.40 + chapter * 0.035);
	vec2 planet_uv = (uv - planet_center) * vec2(1.0, 1.78);
	float planet_radius = 0.39 - chapter * 0.025;
	float planet_distance = length(planet_uv);
	float planet_mask = 1.0 - smoothstep(planet_radius - 0.003, planet_radius + 0.003, planet_distance);
	float day_side = smoothstep(-0.42, 0.72, planet_uv.x / planet_radius);
	vec3 planet_dark = vec3(0.008, 0.015, 0.022);
	vec3 planet_light = mix(vec3(0.10, 0.18, 0.23), accent_color.rgb * 0.48, 0.55);
	color = mix(color, mix(planet_dark, planet_light, day_side), planet_mask);

	float rim = exp(-abs(planet_distance - planet_radius) * 180.0) * smoothstep(-0.32, 0.75, planet_uv.x / planet_radius);
	color += accent_color.rgb * rim * 1.25;

	float orbit_a = ring_line(uv, vec2(0.73, 0.54), vec2(1.0, 1.85), 0.44, 0.0011);
	float orbit_b = ring_line(uv, vec2(0.68, 0.51), vec2(1.0, 2.45), 0.34, 0.0008);
	float orbit_fade = smoothstep(0.46, 0.62, uv.x);
	color += accent_color.rgb * (orbit_a * 0.17 + orbit_b * 0.10) * orbit_fade;

	float horizon = 1.0 - smoothstep(0.0, 0.0022, abs(uv.y - 0.835));
	float tick = step(0.84, fract(uv.x * 42.0));
	color += accent_color.rgb * horizon * tick * 0.11;

	COLOR = vec4(color, 1.0);
}
