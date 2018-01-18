#version 330 core

uniform mat4 mvp;

uniform float size;

in vec3 vertex_position_model;

out vec3 fragment_position_world;

void main() {
	gl_Position = mvp * vec4(vertex_position_model * size, 1);
	fragment_position_world = vertex_position_model;
}
