#version 330 core

uniform vec3 light_world;
uniform vec3 light_color;
uniform float light_power;
uniform float light_radius;

in vec3 fragment_position_world;
in vec3 fragment_color;
in vec3 fragment_normal_camera;
in vec3 fragment_toeye_camera;
in vec3 fragment_tolight_camera;

out vec3 output_color;

void main() {
	vec3 diffuse_color = fragment_color;
	vec3 ambient_color = 0.15 * diffuse_color;
	vec3 specular_color = vec3(.1, .1, .1);

	output_color = ambient_color;

	vec3 n = normalize(fragment_normal_camera);
	/* light */ {
		float light_distance = length(light_world - fragment_position_world);
		vec3 l = normalize(fragment_tolight_camera);
		float cos_theta = clamp(dot(n, l), 0, 1);
		vec3 r = reflect(-l, n);
		vec3 e = normalize(fragment_toeye_camera);
		float cos_alpha = clamp(dot(e, r), 0, 1);

		float dist = max(0, 1 / (light_distance * light_distance) - 1 / (light_radius * light_radius));
		output_color += dist * light_power * light_color * (
			diffuse_color * cos_theta +
			specular_color * pow(cos_alpha, 5)
		);
	}
}
