#version 330 core

uniform mat4 mv;
uniform vec3 light_world;
uniform vec3 light_color;
uniform float light_power;

in vec3 fragment_position_world;
in vec3 fragment_color;
in vec3 fragment_normal_camera;
in vec3 fragment_toeye_camera;
in vec3 fragment_fromlight_camera;

out vec4 output_color;

void main() {
	vec3 diffuse_color = vec3(1, 1, 1);// fragment_color;
	vec3 ambient_color = 0.15 * diffuse_color;
	vec3 specular_color = vec3(.1, .1, .1);

	float light_distance = length(light_world - fragment_position_world);
	vec3 n = normalize(fragment_normal_camera);
	vec3 l = normalize(fragment_fromlight_camera);
	float cosTheta = clamp(dot(n, l), 0, 1);
	vec3 e = normalize(fragment_toeye_camera);
	vec3 r = reflect(-l, n);
	float cosAlpha = clamp(dot(e, r), 0, 1);

	vec3 color;
	color =
		ambient_color
		+ diffuse_color * light_color * light_power * cosTheta / (light_distance * light_distance)
		+ specular_color * light_color * light_power * pow(cosAlpha, 5) / (light_distance * light_distance);

//	color = diffuse_color; // No light
//	color = gl_FragCoord.zzz; // Limbo

	output_color = vec4(color, 1);
}
