#pragma once

#include <epoxy/gl.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <ostream>
#include <string>
#include <vector>

class Object {
public:
	struct vertex_data {
		glm::vec3 pos;
		glm::vec3 norm;
		glm::vec3 color;
	};

	static Object load(std::string const &obj);
	friend std::ostream &operator<<(std::ostream &s, Object const &o);

private:
	Object() = default;

public:
	std::vector<vertex_data> verticies;
	std::vector<std::array<GLuint, 3>> faces;
	
	void recalculate_normals();
	void normals_as_colors();
};

std::ostream &operator<<(std::ostream &s, Object const &o);
