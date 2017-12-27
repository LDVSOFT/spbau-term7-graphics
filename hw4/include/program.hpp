#pragma once

#include <epoxy/gl.h>

#include <string>
#include <memory>

class Program {
private:
	GLuint id;

	static GLuint build_shader(GLenum type, std::string const &source, std::string &error);

public:
	static GLuint constexpr no_id{static_cast<GLuint>(-1)};

	static std::unique_ptr<Program> build_program(std::initializer_list<std::tuple<GLenum, std::string>> sources, std::string &error);

	Program(GLuint program);
	~Program();

	GLuint get_uniform(std::string const &name) const;
	GLuint get_attribute(std::string const &name) const;

	void use() const;
};
