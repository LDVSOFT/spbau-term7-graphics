#version 330 core

in vec3 fragment_light_sphere_color;

out vec3 color;

void main() {
	color = fragment_light_sphere_color;
}
