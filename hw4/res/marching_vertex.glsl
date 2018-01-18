#version 330 core

uniform mat4 m;
uniform mat4 mvp;

uniform vec3 eye_world;

in vec3 vertex_position_model;
in vec3 vertex_normal_model;

out vec3 fragment_fromeye_world;
out vec3 fragment_normal_world;
out vec3 fragment_color;

void main() {
	gl_Position = mvp * vec4(vertex_position_model, 1);
	fragment_fromeye_world = (m * vec4(vertex_position_model, 1)).xyz - eye_world;
	fragment_normal_world = (m * vec4(vertex_normal_model, 0)).xyz;
	fragment_color = (vertex_position_model + vec3(1, 1, 1)) / 2 *
		(dot(vec3(0, 1, 0), fragment_normal_world) + 1.25) / 2.5;
}
