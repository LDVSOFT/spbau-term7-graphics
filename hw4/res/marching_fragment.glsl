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

	float index_from = 1;
	float index_to = refract_index;
	vec3 reflect_to = normalize(reflect(fragment_fromeye_world, n));
	vec3 refract_to = normalize(refract(fragment_fromeye_world, n, index_from / index_to));

	/* from program: */
	//float reflect_coef = reflect_power;
	//float refract_coef = refract_power;

	/* fresnel: */
	float cos_theta_from = dot(reflect_to, +n); // == dot(eye_to, +n)
	float cos_theta_to   = dot(refract_to, -n);
	float f_r_parl = pow((index_to * cos_theta_from - index_from * cos_theta_to) / (index_to * cos_theta_from + index_from * cos_theta_to) , 2);
	float f_r_perp = pow((index_to * cos_theta_from - index_from * cos_theta_to) / (index_from * cos_theta_from + index_to * cos_theta_to) , 2);
	float reflect_coef = (reflect_power + refract_power) * (f_r_parl + f_r_perp) / 2;
	//float reflect_coef = reflect_power * pow(1 - cos_theta_from, 5 * refract_power);
	float refract_coef = (reflect_power + refract_power) - reflect_coef;

	output_color += texture(skybox, reflect_to).xyz * reflect_coef;
	output_color += texture(skybox, refract_to).xyz * refract_coef;

	//output_color = vec3(1, 1, 1) * reflect_coef;
}
