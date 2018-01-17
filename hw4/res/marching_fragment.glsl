#version 330 core

in vec3 fragment_color;

out vec3 output_color;

void main() {
	output_color = fragment_color;
}
