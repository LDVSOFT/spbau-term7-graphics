#version 330 core

uniform mat4 m;
uniform mat4 mvp;

uniform int mode;

in vec3 vertex_position_model;
in vec3 vertex_normal_model;
in vec3 vertex_color;

out vec3 fragment_color;

void main() {
	gl_Position = mvp * vec4(vertex_position_model, 1);

	if (mode == 0) {
		vec3 normal_world = normalize((m * vec4(vertex_normal_model, 0)).xyz);
		fragment_color = vec3(1, 1, 1) *
			(dot(vec3(0, 1, 0), normal_world) + 1.25) / 2.5;
		/*  \______________________[-1, 1]_/
		 *                         \___[.25, 2.25]_/
		 *                             \________[.1, .9]_/
		 */
	}
	if (mode == 1)
		fragment_color = vertex_color;
}
