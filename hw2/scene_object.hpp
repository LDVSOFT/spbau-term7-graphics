#pragma once

#include "libs.hpp"

#include "object.hpp"

class SceneObject {
private:
	GLuint vao{0};
	GLuint elems{0}, data{0};
	size_t elems_count;

public:
	glm::mat4 position{1.0};

	SceneObject(Object const &obj);
	~SceneObject();

	static constexpr GLuint no_attribute = -1;
	void draw(
		glm::mat4 const &v, glm::mat4 const &p,
		GLuint m_attribute, GLuint v_attribute, GLuint p_attribute,
		GLuint mv_attribute, GLuint mvp_attribute
	) const;

	void set_attribute_to_position(GLuint attribute) const;
	void set_attribute_to_normal(GLuint attribute) const;
	void set_attribute_to_uv(GLuint attribute) const;
};
