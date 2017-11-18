#version 330 core

uniform mat4 mv;
uniform mat4 v;
uniform vec3 light_world;
uniform vec3 light_color;
uniform float light_power;
uniform vec3 tosun_world;
uniform vec3 sun_color;
uniform float sun_power;

uniform sampler2D shadowmap;

in vec3 fragment_position_world;
in vec3 fragment_color;
in vec3 fragment_normal_camera;
in vec3 fragment_toeye_camera;
in vec3 fragment_tolight_camera;
in vec3 fragment_shadowmap_position;

out vec3 output_color;

void main() {
//	vec3 diffuse_color = vec3(1, 1, 1);
	vec3 diffuse_color = fragment_color;
	vec3 ambient_color = 0.15 * diffuse_color;
	vec3 specular_color = vec3(.1, .1, .1);

	output_color = ambient_color;

	vec3 n = normalize(fragment_normal_camera);
	vec3 tosun_camera = (v * vec4(tosun_world, 0)).xyz;
	/* light */ {
		float light_distance = length(light_world - fragment_position_world);
		vec3 l = normalize(fragment_tolight_camera);
		float cos_theta = clamp(dot(n, l), 0, 1);
		vec3 r = reflect(-l, n);
		vec3 e = normalize(fragment_toeye_camera);
		float cos_alpha = clamp(dot(e, r), 0, 1);

		output_color += 1 * (
			diffuse_color * light_color * light_power * cos_theta / (light_distance * light_distance) +
			specular_color * light_color * light_power * pow(cos_alpha, 5) / (light_distance * light_distance)
		);
	}

	/* sun */ {
		vec3 l = normalize(tosun_camera);
		float cos_theta = clamp(dot(n, l), 0, 1);
		vec3 r = reflect(-l, n);
		vec3 e = normalize(tosun_camera);
		float cos_alpha = clamp(dot(e, r), 0, 1);

		float visibility = 1;
		float shadowmap_distance = texture(shadowmap, fragment_shadowmap_position.xy).r;
		float actual_distance = fragment_shadowmap_position.z;
		float bias = clamp(.0001 * tan(acos(cos_alpha)), 0, .001);
		if (shadowmap_distance < actual_distance - .0001) {
			visibility *= .1;
		}

		/* debug */ {
			//visibility = 0;
			//output_color = vec3((actual_distance - 0.49) / 0.03, (shadowmap_distance - 0.49) / 0.03, 0.1);
			//output_color = vec3(texture(shadowmap, vec2(.5, .5)).r, 0, (actual_distance - 0.49) / 0.03);
			//output_color = vec3(fragment_shadowmap_position.xy, 0);
			//output_color = vec3(fragment_shadowmap_position.xy, (actual_distance - 0.49) / 0.03);
		}

		output_color += visibility * (
			diffuse_color * sun_color * sun_power * cos_theta +
			specular_color * sun_color * sun_power * pow(cos_alpha, 5)
		);
	}

//	output_color = diffuse_color; // No light
//	output_color = gl_FragCoord.zzz; // Limbo
}
