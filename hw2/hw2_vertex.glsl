#version 130

uniform mat4 mvp;

in vec3 vertex_position;
in vec3 vertex_color;

out vec4 clr;

void main() {
	gl_Position = mvp * vec4(vertex_position.xyz, 1.0);
	clr = vec4(vertex_color, .5);
}
