#version 330 core

uniform sampler2D albedo;
uniform sampler2D normal;
uniform sampler2D depth;
uniform int id;

in vec2 fragment_position;

out vec3 output_color;

void main() {
	gl_FragDepth = 1;
	if (id == 0)
		output_color = texture(albedo, fragment_position).rgb;
	if (id == 1)
		output_color = texture(normal, fragment_position).xyz / 2 + vec3(.5, .5, .5);
	if (id == 2)
		output_color = vec3(1, 1, 1) * texture(depth, fragment_position).r;
	if (id == 3) {
		output_color = texture(albedo, fragment_position).rgb * .15;
		gl_FragDepth = texture(depth , fragment_position).r;
	}
}
