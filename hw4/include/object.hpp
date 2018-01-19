#pragma once

#define GLM_FORCE_SWIZZLE

#include <glm/glm.hpp>

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
	static Object manual(std::vector<vertex_data> const &data, std::vector<glm::uvec3> const &elems);
	friend std::ostream &operator<<(std::ostream &s, Object const &o);

private:
	Object() = default;

public:
	std::vector<vertex_data> verticies;
	std::vector<glm::uvec3> faces;
	
	void recalculate_normals();
	void normals_as_colors();
};

std::ostream &operator<<(std::ostream &s, Object const &o);
