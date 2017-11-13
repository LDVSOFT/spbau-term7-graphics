#version 330 core

uniform mat4 mv;
uniform mat4 v;
uniform vec3 light_world;
uniform vec3 light_color;
uniform float light_power;

uniform vec3 tosun_world;
uniform vec3 sun_color;
uniform float sun_power;

in vec3 fragment_position_world;
in vec3 fragment_color;
in vec3 fragment_normal_camera;
in vec3 fragment_toeye_camera;
in vec3 fragment_tolight_camera;

out vec3 output_color;

void main() {
//	vec3 diffuse_color = vec3(1, 1, 1);
	vec3 diffuse_color = fragment_color;
	vec3 ambient_color = 0.15 * diffuse_color;
	vec3 specular_color = vec3(.1, .1, .1);

	output_color = ambient_color;

	vec3 n = normalize(fragment_normal_camera);
	vec3 e = normalize(fragment_toeye_camera);
	vec3 tosun_camera = (v * vec4(tosun_world, 0)).xyz;
	/* light */ {
		float light_distance = length(light_world - fragment_position_world);
		vec3 l = normalize(fragment_tolight_camera);
		float cos_theta = clamp(dot(n, l), 0, 1);
		vec3 r = reflect(-l, n);
		float cos_alpha = clamp(dot(e, r), 0, 1);

		output_color +=
			diffuse_color * light_color * light_power * cos_theta / (light_distance * light_distance) +
			specular_color * light_color * light_power * pow(cos_alpha, 5) / (light_distance * light_distance);
	}

	/* sun */ {
		vec3 l = normalize(tosun_camera);
		float cos_theta = clamp(dot(n, l), 0, 1);
		vec3 r = reflect(-l, n);
		float cos_alpha = clamp(dot(e, r), 0, 1);

		output_color +=
			diffuse_color * sun_color * sun_power * cos_theta +
			specular_color * sun_color * sun_power * pow(cos_alpha, 5);
	}

//	output_color = diffuse_color; // No light
//	output_color = gl_FragCoord.zzz; // Limbo
}
