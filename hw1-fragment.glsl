#version 130

uniform int iterations;
uniform sampler1D colorizer;
uniform int colorizer_period;

smooth in highp vec2 planePosition;

out vec4 outputColor;

#define cx_mul(a, b) vec2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x)

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
			float h = log(float(it) + 1) * 20;
			outputColor = vec4(texture(colorizer, h / colorizer_period).rgb, 1);
			return;
		}
	}
	outputColor = insideColor;
}
