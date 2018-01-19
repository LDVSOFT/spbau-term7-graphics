#pragma once

#define GLM_FORCE_SWIZZLE

#include "object.hpp"

#include <epoxy/gl.h>
#include <glm/glm.hpp>

class SceneObject {
private:
	GLuint vao{0};
	GLuint elems{0}, data{0};
	size_t elems_count;

public:
	glm::mat4 position{1.0}, animation_position{1.0};

	explicit SceneObject(Object const &obj);
	SceneObject(SceneObject const &other) = delete;
	~SceneObject();

	void draw(
		glm::mat4 const &v, glm::mat4 const &p,
		GLuint m_attribute, GLuint mv_attribute, GLuint mvp_attribute,
		GLuint m_inv_attribute, GLuint mv_inv_attribute, GLuint mvp_inv_attribute
	) const;

	void set_attribute_to_position(GLuint attribute) const;
	void set_attribute_to_normal(GLuint attribute) const;
	void set_attribute_to_color(GLuint attribute) const;
};
