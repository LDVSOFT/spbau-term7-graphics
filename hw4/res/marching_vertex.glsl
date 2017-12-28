#version 330 core

uniform float a;
uniform vec3 c;

uniform float threshold;
uniform float dx, dy, dz;

in vec3 vertex_position_world;

out vec4 geometry_value0123, geometry_value4567;
out int geometry_case_id;

float f(float a, vec3 c, vec3 p) {
	return a / pow(distance(c, p), 2);
}

float F(vec3 p) {
	return f(a, c, p) - threshold;
}

void main() {
	// geometry shader will produce screen coordinates
	gl_Position = vec4(vertex_position_world, 1); 

	geometry_value0123 = vec4(
		F(vertex_position_world + vec3(-dx, -dy, -dz)),
		F(vertex_position_world + vec3(+dx, -dy, -dz)),
		F(vertex_position_world + vec3(-dx, +dy, -dz)),
		F(vertex_position_world + vec3(+dx, +dy, -dz))
	);
	geometry_value4567 = vec4(
		F(vertex_position_world + vec3(-dx, -dy, +dz)),
		F(vertex_position_world + vec3(+dx, -dy, +dz)),
		F(vertex_position_world + vec3(-dx, +dy, +dz)),
		F(vertex_position_world + vec3(+dx, +dy, +dz))
	);

	vec4 zero4 = vec4(0, 0, 0, 0);
	ivec4 sign0123 = ivec4(greaterThanEqual(geometry_value0123, zero4));
	ivec4 sign4567 = ivec4(greaterThanEqual(geometry_value4567, zero4));
	geometry_case_id = 0
		| (sign0123.x << 0)
		| (sign0123.y << 1)
		| (sign0123.z << 2)
		| (sign0123.w << 3)
		| (sign4567.x << 4)
		| (sign4567.y << 5)
		| (sign4567.z << 6)
		| (sign4567.w << 7);
}
