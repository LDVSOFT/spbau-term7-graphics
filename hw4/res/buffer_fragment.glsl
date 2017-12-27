#version 330 core

in vec3 fragment_color;
in vec3 fragment_normal_world;

layout(location = 0) out vec3 output_aldego;
layout(location = 1) out vec3 output_normal_world;
/* implicit: depth */

void main() {
	output_aldego = fragment_color;
	output_normal_world = normalize(fragment_normal_world);
}
