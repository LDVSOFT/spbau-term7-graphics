#version 330 core

in float height;

out vec3 output_color;

void main() {
	output_color = vec3(1, height, height);
}
