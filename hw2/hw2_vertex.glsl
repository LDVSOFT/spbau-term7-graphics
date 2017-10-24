#version 130

uniform mat4 mvp;

in vec2 position;

void main() {
	gl_Position = mvp * vec4(position.xy, 0.0, 1.0);
//	gl_Position = vec4(position.xy, 0.0, 1.0);
}
