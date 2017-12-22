#version 330 core

uniform vec3 light_color;

in vec3 fragment_light_sphere_color;

out vec3 color;

void main() {
	//color = gl_FragCoord.xyz * .01;
	color = light_color * fragment_light_sphere_color;
}
