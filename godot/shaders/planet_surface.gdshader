shader_type spatial;
render_mode diffuse_burley, specular_schlick_ggx;

uniform vec4 land_color : source_color = vec4(0.20, 0.32, 0.24, 1.0);
uniform vec4 highland_color : source_color = vec4(0.55, 0.48, 0.34, 1.0);
uniform vec4 ocean_color : source_color = vec4(0.02, 0.08, 0.13, 1.0);
uniform vec4 ice_color : source_color = vec4(0.82, 0.91, 0.95, 1.0);
uniform vec4 cloud_color : source_color = vec4(0.82, 0.88, 0.91, 1.0);
uniform float seed = 1.0;
uniform float gas_giant : hint_range(0.0, 1.0) = 0.0;
uniform float cloud_cover : hint_range(0.0, 1.0) = 0.0;
uniform float emission_strength : hint_range(0.0, 0.5) = 0.075;
uniform float rim_fill : hint_range(0.0, 0.5) = 0.0;

varying vec3 object_position;

float hash31(vec3 p) {
	p = fract(p * 0.1031);
	p += dot(p, p.yzx + 33.33);
	return fract((p.x + p.y) * p.z);
}

float value_noise(vec3 p) {
	vec3 cell = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float n000 = hash31(cell + vec3(0.0, 0.0, 0.0));
	float n100 = hash31(cell + vec3(1.0, 0.0, 0.0));
	float n010 = hash31(cell + vec3(0.0, 1.0, 0.0));
	float n110 = hash31(cell + vec3(1.0, 1.0, 0.0));
	float n001 = hash31(cell + vec3(0.0, 0.0, 1.0));
	float n101 = hash31(cell + vec3(1.0, 0.0, 1.0));
	float n011 = hash31(cell + vec3(0.0, 1.0, 1.0));
	float n111 = hash31(cell + vec3(1.0, 1.0, 1.0));
	return mix(
		mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y),
		mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y),
		f.z
	);
}

float fbm(vec3 p) {
	float value = 0.0;
	float amplitude = 0.52;
	for (int octave = 0; octave < 5; octave++) {
		value += value_noise(p) * amplitude;
		p = p * 2.03 + vec3(7.1, 3.7, 5.4);
		amplitude *= 0.48;
	}
	return value;
}

void vertex() {
	object_position = VERTEX;
}

void fragment() {
	vec3 sphere = normalize(object_position);
	float continents = fbm(sphere * 3.8 + vec3(seed));
	float detail = fbm(sphere * 12.0 - vec3(seed * 0.37));
	float latitude = abs(sphere.y);
	float land_mask = smoothstep(0.49, 0.58, continents + detail * 0.11);
	vec3 rock = mix(land_color.rgb, highland_color.rgb, smoothstep(0.54, 0.79, detail));
	vec3 terrestrial = mix(ocean_color.rgb, rock, land_mask);
	terrestrial = mix(terrestrial, ice_color.rgb, smoothstep(0.76, 0.94, latitude + detail * 0.08));
	float cloud_noise = fbm(sphere * 7.4 + vec3(seed * 2.7));
	float cloud_wisp = smoothstep(0.56, 0.73, cloud_noise + detail * 0.08) * cloud_cover;
	terrestrial = mix(terrestrial, cloud_color.rgb, cloud_wisp * 0.72);

	float bands = sin((sphere.y + detail * 0.055) * 47.0 + seed) * 0.5 + 0.5;
	float storm = smoothstep(0.58, 0.83, fbm(sphere * vec3(2.0, 8.0, 2.0) + vec3(seed)));
	vec3 giant = mix(ocean_color.rgb, highland_color.rgb, 0.22 + bands * 0.66);
	giant = mix(giant, land_color.rgb, storm * 0.40);

	ALBEDO = mix(terrestrial, giant, gas_giant);
	float view_rim = pow(clamp(1.0 - abs(dot(normalize(NORMAL), normalize(VIEW))), 0.0, 1.0), 2.6);
	vec3 rim_color = mix(vec3(0.04, 0.11, 0.18), ALBEDO, 0.38);
	EMISSION = ALBEDO * emission_strength + rim_color * view_rim * rim_fill;
	ROUGHNESS = mix(mix(0.24, 0.78, land_mask), 0.68, gas_giant);
	METALLIC = 0.03;
	SPECULAR = mix(0.72, 0.30, land_mask);
}
