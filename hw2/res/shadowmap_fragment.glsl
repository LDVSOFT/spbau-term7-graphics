#version 330 core

in vec3 fragment_shadowmap_position;

out vec3 output_color;

void main() {
	output_color = gl_FragCoord.zzz;
}
