#version 330 core

uniform mat4 mvp;

in vec3 vertex_position_model;

out float height;

void main() {
	gl_Position = mvp * vec4(vertex_position_model, 1);
	height = (vertex_position_model.y + 1) / 2;
}
