#version 130

uniform vec2 center;
uniform vec2 zoom;

in vec2 position;

smooth out highp vec2 planePosition;

void main() {
	gl_Position = vec4(position, 1.0, 1.0);
	planePosition = vec2(center) + vec2(position) * zoom;
}
