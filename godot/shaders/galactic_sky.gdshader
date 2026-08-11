shader_type sky;

const vec3 GALACTIC_NORTH = vec3(-0.86766615, 0.45598378, 0.19807637);
const vec3 GALACTIC_CENTER = vec3(-0.05487396, -0.48383503, 0.87343718);

float hash31(vec3 p) {
	p = fract(p * 0.1031);
	p += dot(p, p.yzx + 33.33);
	return fract((p.x + p.y) * p.z);
}

float noise3(vec3 p) {
	vec3 cell = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(
		mix(mix(hash31(cell), hash31(cell + vec3(1.0, 0.0, 0.0)), f.x),
			mix(hash31(cell + vec3(0.0, 1.0, 0.0)), hash31(cell + vec3(1.0, 1.0, 0.0)), f.x), f.y),
		mix(mix(hash31(cell + vec3(0.0, 0.0, 1.0)), hash31(cell + vec3(1.0, 0.0, 1.0)), f.x),
			mix(hash31(cell + vec3(0.0, 1.0, 1.0)), hash31(cell + vec3(1.0)), f.x), f.y), f.z
	);
}

float fbm(vec3 p) {
	float value = 0.0;
	float weight = 0.54;
	for (int octave = 0; octave < 5; octave++) {
		value += noise3(p) * weight;
		p = p * 2.07 + vec3(5.7, 1.3, 8.9);
		weight *= 0.47;
	}
	return value;
}

void sky() {
	vec3 direction = normalize(EYEDIR);
	float galactic_latitude = abs(dot(direction, GALACTIC_NORTH));
	float broad_plane = exp(-galactic_latitude * 9.5);
	float narrow_plane = exp(-galactic_latitude * 31.0);
	float toward_center = max(dot(direction, GALACTIC_CENTER), 0.0);
	float bulge = pow(toward_center, 5.0) * exp(-galactic_latitude * 7.0);
	float clouds = fbm(direction * 5.2);
	float fine_clouds = fbm(direction * 15.0 + vec3(4.0, 9.0, 2.0));
	float stellar_haze = broad_plane * (0.32 + clouds * 0.68) + bulge * 1.45;
	float dust = narrow_plane * smoothstep(0.47, 0.79, fine_clouds);
	vec3 base = vec3(0.00035, 0.00065, 0.00125);
	vec3 warm_haze = mix(vec3(0.010, 0.017, 0.028), vec3(0.042, 0.032, 0.026), toward_center);
	vec3 color = base + warm_haze * stellar_haze;
	color *= 1.0 - dust * 0.74;
	COLOR = color;
}
