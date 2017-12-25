#version 330 core

uniform mat4 m;
uniform mat4 mvp;

in vec3 vertex_position_model;
in vec3 vertex_normal_model;
in vec3 vertex_color;

out vec3 fragment_color;
out vec3 fragment_normal_world;

void main() {
	gl_Position = mvp * vec4(vertex_position_model, 1);

	fragment_color = vertex_color;
	fragment_normal_world = (m * vec4(vertex_normal_model, 0)).xyz;
}
