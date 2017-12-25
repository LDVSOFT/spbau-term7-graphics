#version 330 core

in vec3 vertex_position_model;

out vec2 fragment_position;

void main() {
	gl_Position = vec4(vertex_position_model, 1);
	fragment_position = (vertex_position_model.xy + vec2(+1, +1)) / 2;
/*                       \____________[-1, 1]^2_/
 *                                    \________________[0, 2]^2_/
 *                                                     \___[0, 1]^2_/
 */
}
