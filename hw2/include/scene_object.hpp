#pragma once

#include "object.hpp"

#include <epoxy/gl.h>
#include <glm/glm.hpp>

#include <set>

class SceneObject {
private:
	GLuint vao{0};
	GLuint elems{0}, data{0};
	size_t elems_count;

public:
	glm::mat4 position{1.0};

	SceneObject(Object const &obj);
	~SceneObject();

	void draw(
		glm::mat4 const &v, glm::mat4 const &p,
		GLuint m_attribute, GLuint v_attribute, GLuint p_attribute,
		GLuint mv_attribute, GLuint mvp_attribute
	) const;

	void set_attribute_to_position(GLuint attribute) const;
	void set_attribute_to_normal(GLuint attribute) const;
	void set_attribute_to_color(GLuint attribute) const;
};
