#version 330 core

uniform mat4 mvp;

in vec3 vertex_position_model;

out vec3 fragment_color;

void main() {
	gl_Position = mvp * vec4(vertex_position_model, 1);
	fragment_color = (vertex_position_model + vec3(1, 1, 1)) / 2;
}
