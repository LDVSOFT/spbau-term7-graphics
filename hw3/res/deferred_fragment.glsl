#version 330 core

uniform mat4 v;
uniform mat4 vp;
uniform mat4 p_inv;
uniform mat4 vp_inv;

uniform sampler2D albedo_texture;
uniform sampler2D normal_texture;
uniform sampler2D depth_texture;

uniform vec3 light_world;
uniform vec3 light_color;
uniform float light_power;
uniform float light_radius;

out vec3 output_color;

void main() {
	vec2 size = textureSize(albedo_texture, 0);
	vec2 position_in_texture = gl_FragCoord.xy / size;
	vec3  albedo            = texture(  albedo_texture, position_in_texture).rgb;
	vec3  normal_world      = texture(  normal_texture, position_in_texture).xyz;
	float depth_camera_norm = texture(   depth_texture, position_in_texture).r;
	if (depth_camera_norm == 1.0)
		discard;

	vec3 position_proj = vec3(position_in_texture, depth_camera_norm) * 2 - vec3(1, 1, 1);
	gl_FragDepth = position_proj.z;
	vec4 position_world_pre = vp_inv * vec4(position_proj, 1);
	vec4 position_camera_pre = p_inv * vec4(position_proj, 1);
	position_world_pre /= position_world_pre.w;
	position_camera_pre /= position_camera_pre.w;

	vec3 position_world = position_world_pre.xyz;
	vec3 position_camera = position_camera_pre.xyz;

	float light_distance = length(position_world - light_world);
	if (light_distance > light_radius)
		discard;

	vec3 normal_camera = normalize((v * vec4(normal_world, 0)).xyz);
	vec3 light_camera = (v * vec4(light_world, 1)).xyz;
	vec3 toeye_camera = -position_camera;
	vec3 tolight_camera = light_camera - position_camera;

	vec3 diffuse_color = albedo;
	vec3 specular_color = vec3(.1, .1, .1);

	vec3 l = normalize(tolight_camera);
	float cos_theta = clamp(dot(normal_camera, l), 0, 1);
	vec3 r = reflect(-l, normal_camera);
	vec3 e = normalize(toeye_camera);
	float cos_alpha = clamp(dot(e, r), 0, 1);

	float dist = 1 / (light_distance * light_distance) - 1 / (light_radius * light_radius);
	output_color = dist * light_power * light_color * (
		diffuse_color * cos_theta +
		specular_color * pow(cos_alpha, 5)
	);
}
