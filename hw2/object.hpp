#pragma once

#include "libs.hpp"

class Object {
public:
	struct vertex_data {
		glm::vec3 pos;
		glm::vec3 norm;
		glm::vec2 uv;
	};

	static Object load(std::string const &obj);
	friend std::ostream &operator<<(std::ostream &s, Object const &o);

private:
	Object() = default;

public:
	std::vector<vertex_data> verticies;
	std::vector<std::array<GLuint, 3>> faces;
	
	void recalculate_normals();
};

std::ostream &operator<<(std::ostream &s, Object const &o);
