#version 330 core

uniform mat4 mvp;

in vec3 vertex_position_model;

void main() {
	gl_Position = mvp * vec4(vertex_position_model, 1);
}
