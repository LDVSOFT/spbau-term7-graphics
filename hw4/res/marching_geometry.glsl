#version 330 core

#define MAX_TRIANGLES 5
#define MAX_VERTICES 15

uniform mat4 vp;

uniform float dx, dy, dz;

uniform sampler1D geometry_base_positions;
uniform isampler1D geometry_edges;
uniform isampler1D geometry_case_sizes;

layout(points) in;
layout(triangle_strip, max_vertices = MAX_VERTICES) out;

in from_vertex {
	vec4 geometry_value0123, geometry_value4567;
	int geometry_case_id;
} gl_vs[];

void main() {
	vec3 position  = gl_in[0].gl_Position.xyz;
	vec4 value0123 = gl_vs[0].geometry_value0123;
	vec4 value4567 = gl_vs[0].geometry_value4567;
	int  id        = gl_vs[0].geometry_case_id;

	int case_size = texelFetch(geometry_case_sizes, id, 0).x;
	for (int i = 0; i < case_size; ++i) {
		for (int j = 0; j < 3; ++j) {
			int edge_id = (id * MAX_TRIANGLES + i) * 3 + j;
			int edge = texelFetch(geometry_edges, edge_id, 0).x;
			vec3 base_position = texelFetch(geometry_base_positions, edge, 0).xyz;
			gl_Position = vp * vec4(position + vec3(dx, dy, dz) * base_position, 1);
			EmitVertex();
		}
		EndPrimitive();
	}
}
