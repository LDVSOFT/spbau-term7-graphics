#version 330 core

uniform samplerCube skybox;

in vec3 fragment_position_world;

out vec3 output_color;

void main() {
	output_color = texture(skybox, fragment_position_world).xyz;
}
