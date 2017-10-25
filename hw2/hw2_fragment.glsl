#version 130

in vec4 clr;

out vec4 colorOutput;

void main() {
	colorOutput = clr;
//	colorOutput = vec4(gl_FragCoord.zzz, clr.w); // Limbo
}
