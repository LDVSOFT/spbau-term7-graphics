#pragma once

#include <glm/glm.hpp>

namespace gl_data {
	extern const size_t MAX_TRIANGLES;

	extern size_t const base_positions_count;
	extern glm::vec3 const base_positions[];

	extern size_t const edges_count;
	extern uint8_t const edges[];

	extern size_t const cases_count;;
	extern uint8_t case_sizes[];
}
