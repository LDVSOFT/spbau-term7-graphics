#version 330 core

out float output_depth;
//out vec3 output_color;

void main() {
	output_depth = gl_FragCoord.z;
//	output_color = gl_FragCoord.zzz;
}
