#version 130

in vec4 clr;

out vec4 colorOutput;

void main() {
//	colorOutput = clr;
	colorOutput = (clr + vec4(1,1,1,0)) / 2;// Normals as color
//	colorOutput = vec4(gl_FragCoord.zzz, clr.w); // Limbo
}
