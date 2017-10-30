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

	void draw(GLuint mvp_attribute, glm::mat4 const &vp) const;
	
	void set_attribute_to_position(GLuint attribute) const;
	void set_attribute_to_normal(GLuint attribute) const;
	void set_attribute_to_uv(GLuint attribute) const;
};
