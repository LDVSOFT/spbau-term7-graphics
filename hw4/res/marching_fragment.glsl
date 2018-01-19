#version 330 core

uniform float color_power;
uniform float reflect_power;
uniform float refract_power;
uniform float refract_index;

uniform samplerCube skybox;

in vec3 fragment_fromeye_world;
in vec3 fragment_normal_world;
in vec3 fragment_color;

out vec3 output_color;

void main() {
	output_color = vec3(0, 0, 0);
	vec3 n = normalize(fragment_normal_world);

	output_color += fragment_color * color_power;

	vec3 reflect_to = reflect(fragment_fromeye_world, n);
	output_color += texture(skybox, reflect_to).xyz * reflect_power;

	vec3 refract_to = refract(fragment_fromeye_world, n, 1 / refract_index);
	output_color += texture(skybox, refract_to).xyz * refract_power;
}
