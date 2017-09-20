#version 130

uniform int iterations;

smooth in highp vec2 planePosition;

out vec4 outputColor;

#define cx_mul(a, b) vec2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x)

vec3 hsv2rgb(vec3 c) {
	vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
	vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
	return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
	highp vec2 c = planePosition;
	vec4 insideColor = vec4(0, 0, 0, 1);

	/* skip main cardiod */ {
		float phi = atan(c.y, c.x - .25);
		float rho_c = .5 - cos(phi) / 2;
		float rho = length(c - vec2(.25, 0));
		if (rho < rho_c) {
			outputColor = insideColor;
			return;
		}
	}

	highp vec2 z = vec2(0, 0);
	int it;
	for (it = 0; it < iterations; ++it) {
		z = cx_mul(z, z) + c;
		if (z.x * z.x + z.y * z.y > 4) {
			float h = it == 0 ? 0 : mod(log(float(it)) * 20, 360) / 360;
			outputColor = vec4(hsv2rgb(vec3(h, 1, 1)), 1);
			return;
		}
	}
	outputColor = insideColor;
}
