#version 330 core

uniform mat4 mvp;

in vec3 vertex_position_model;

out vec3 fragment_shadowmap_position;

void main() {
	gl_Position = mvp * vec4(vertex_position_model, 1);
	fragment_shadowmap_position = gl_Position.xyz / 2 + vec3(.5, .5, .5);
}
