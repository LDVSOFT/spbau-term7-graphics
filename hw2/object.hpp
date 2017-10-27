#pragma once

#include <glm/glm.hpp>

#include <vector>
#include <string>
#include <ostream>
#include <array>

class object {
public:
	struct vertex_data {
		glm::vec3 pos;
		glm::vec3 norm;
		glm::vec2 uv;
	};

	static object load(std::string const &obj);
	friend std::ostream &operator<<(std::ostream &s, object const &o);

private:
	std::vector<vertex_data> verticies;
	std::vector<std::array<size_t, 3>> faces;

	object() = default;

public:
	glm::mat4 position;

	void recalculate_normals();
};

std::ostream &operator<<(std::ostream &s, object const &o);
