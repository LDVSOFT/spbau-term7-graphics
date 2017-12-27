#include "program.hpp"

#include <vector>

GLuint Program::build_shader(GLenum type, std::string const &source, std::string &error) {
	GLuint shader{glCreateShader(type)};
	const char *s{source.c_str()};
	glShaderSource(shader, 1, &s, nullptr);
	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		GLint length;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
		error.resize(length);
		glGetShaderInfoLog(shader, length, nullptr, &error[0]);

		glDeleteShader(shader);
		return no_id;
	}
	return shader;
}

std::unique_ptr<Program> Program::build_program(std::initializer_list<std::tuple<GLenum, std::string>> sources, std::string &error) {
	std::vector<GLuint> shaders;
	for (auto const &source: sources) {
		GLuint shader{build_shader(std::get<0>(source), std::get<1>(source), error)};
		if (shader == no_id) {
			for (auto i: shaders)
				glDeleteShader(i);
			return nullptr;
		}
		shaders.push_back(shader);
	}

	GLuint program{glCreateProgram()};
	for (auto i: shaders)
		glAttachShader(program, i);
	glLinkProgram(program);
	for (auto i: shaders) {
		glDetachShader(program, i);
		glDeleteShader(i);
	}

	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		GLint length;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
		error.resize(length);
		glGetProgramInfoLog(program, length, nullptr, &error[0]);

		glDeleteProgram(program);
		return nullptr;
	}
	return std::make_unique<Program>(program);
}

Program::Program(GLuint program):
	id{program} {}

Program::~Program() {
	glDeleteProgram(id);
}

GLuint Program::get_uniform(std::string const &name) const {
	return glGetUniformLocation(id, name.c_str());
}

GLuint Program::get_attribute(std::string const &name) const {
	return glGetAttribLocation(id, name.c_str());
}

void Program::use() const {
	glUseProgram(id);
}
