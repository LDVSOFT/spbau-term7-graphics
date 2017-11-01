#version 330 core

uniform mat4 m;
uniform mat4 v;
uniform mat4 mv;
uniform mat4 mvp;
uniform vec3 light_world;

in vec3 vertex_position_model;
in vec3 vertex_normal_model;

out vec3 fragment_position_world;
out vec3 fragment_color;
out vec3 fragment_normal_camera;
out vec3 fragment_toeye_camera;
out vec3 fragment_tolight_camera;

void main() {
	gl_Position = mvp * vec4(vertex_position_model, 1);

	fragment_position_world = (m * vec4(vertex_position_model, 1)).xyz;
	fragment_color = (vertex_normal_model + vec3(1, 1, 1)) / 2; // FIXME
	fragment_normal_camera = (mv * vec4(vertex_normal_model, 0)).xyz;
	fragment_toeye_camera = -(mv * vec4(vertex_position_model, 1)).xyz;
	fragment_tolight_camera = (v * vec4(light_world, 1)).xyz + fragment_toeye_camera;
}
