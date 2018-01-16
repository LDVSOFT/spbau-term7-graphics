#include "scene_object.hpp"
#include "program.hpp"

#include <iostream>

[[maybe_unused]]
static void check(std::string const &name) {
	GLuint error{glGetError()};
	if (error == GL_NO_ERROR)
		std::cout << "Check " << name << " ok" << std::endl;
	else
		std::cout << "Check " << name << " FAILED " << std::hex << error << std::endl;
}

SceneObject::SceneObject(Object const &obj) {
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &data);
	glBindBuffer(GL_ARRAY_BUFFER, data);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Object::vertex_data) * obj.verticies.size(), obj.verticies.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &elems);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elems);
	elems_count = obj.faces.size();
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(decltype(obj.faces)::value_type) * elems_count, obj.faces.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
};

SceneObject::~SceneObject() {
	glDeleteBuffers(1, &data);
	glDeleteBuffers(1, &elems);
	glDeleteVertexArrays(1, &vao);
}

void SceneObject::draw(
	glm::mat4 const &v, glm::mat4 const &p,
	GLuint m_attribute, GLuint mv_attribute, GLuint mvp_attribute,
	GLuint m_inv_attribute, GLuint mv_inv_attribute, GLuint mvp_inv_attribute
) const {
	glBindVertexArray(vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elems);

	glm::mat4 m(position * animation_position), m_inv(glm::inverse(m));
	glm::mat4 mv(v * m), mv_inv(glm::inverse(mv));
	glm::mat4 mvp(p * mv), mvp_inv(glm::inverse(mvp));
	glUniformMatrix4fv(m_attribute      , 1, GL_FALSE, &m      [0][0]);
	glUniformMatrix4fv(mv_attribute     , 1, GL_FALSE, &mv     [0][0]);
	glUniformMatrix4fv(mvp_attribute    , 1, GL_FALSE, &mvp    [0][0]);
	glUniformMatrix4fv(m_inv_attribute  , 1, GL_FALSE, &m_inv  [0][0]);
	glUniformMatrix4fv(mv_inv_attribute , 1, GL_FALSE, &mv_inv [0][0]);
	glUniformMatrix4fv(mvp_inv_attribute, 1, GL_FALSE, &mvp_inv[0][0]);

	glDrawElements(GL_TRIANGLES, (elems_count) * 3, GL_UNSIGNED_INT, nullptr);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void SceneObject::set_attribute_to_position(GLuint attribute) const {
	if (attribute == Program::no_id)
		return;

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, data);
	
	glEnableVertexAttribArray(attribute);
	glVertexAttribPointer(attribute,
		3, GL_FLOAT,
		GL_FALSE,
		sizeof(Object::vertex_data), reinterpret_cast<GLvoid const *>(offsetof(Object::vertex_data, pos))
	);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void SceneObject::set_attribute_to_normal(GLuint attribute) const {
	if (attribute == Program::no_id)
		return;

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, data);
	
	glEnableVertexAttribArray(attribute);
	glVertexAttribPointer(attribute,
		3, GL_FLOAT,
		GL_FALSE,
		sizeof(Object::vertex_data), reinterpret_cast<GLvoid const *>(offsetof(Object::vertex_data, norm))
	);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void SceneObject::set_attribute_to_color(GLuint attribute) const {
	if (attribute == Program::no_id)
		return;

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, data);

	glEnableVertexAttribArray(attribute);
	glVertexAttribPointer(attribute,
		3, GL_FLOAT,
		GL_FALSE,
		sizeof(Object::vertex_data), reinterpret_cast<GLvoid const *>(offsetof(Object::vertex_data, color))
	);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}
