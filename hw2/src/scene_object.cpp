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
//	check("SceneObject:: gen vao");
	glBindVertexArray(vao);
//	check("SceneObject:: bind vao");

	glGenBuffers(1, &data);
//	check("SceneObject:: gen data buffer");
	glBindBuffer(GL_ARRAY_BUFFER, data);
//	check("SceneObject:: bind data buffer");
	glBufferData(GL_ARRAY_BUFFER, sizeof(Object::vertex_data) * obj.verticies.size(), obj.verticies.data(), GL_STATIC_DRAW);
//	check("SceneObject:: set data buffer");

	glGenBuffers(1, &elems);
//	check("SceneObject:: gen elems buffer");
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elems);
//	check("SceneObject:: bind elems buffer");
	elems_count = obj.faces.size();
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(decltype(obj.faces)::value_type) * elems_count, obj.faces.data(), GL_STATIC_DRAW);
//	check("SceneObject:: set elems buffer");

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
	GLuint m_attribute, GLuint v_attribute, GLuint p_attribute, 
	GLuint mv_attribute, GLuint mvp_attribute
) const {
//	check("SceneObject::draw before...");
	glBindVertexArray(vao);
//	check("SceneObject::draw bind vao");
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elems);
//	check("SceneObject::draw bind elems");

	glm::mat4 mv{v * position};
	glm::mat4 mvp{p * mv};
	if (m_attribute != Program::no_id) {
		glUniformMatrix4fv(m_attribute, 1, GL_FALSE, &position[0][0]);
	//	check("SceneObject::draw set m");
	}
	if (v_attribute != Program::no_id) {
		glUniformMatrix4fv(v_attribute, 1, GL_FALSE, &v[0][0]);
	//	check("SceneObject::draw set v");
	}
	if (p_attribute != Program::no_id) {
		glUniformMatrix4fv(p_attribute, 1, GL_FALSE, &p[0][0]);
	//	check("SceneObject::draw set p");
	}
	if (mv_attribute != Program::no_id) {
		glUniformMatrix4fv(mv_attribute, 1, GL_FALSE, &mv[0][0]);
	//	check("SceneObject::draw set mv");
	}
	if (mvp_attribute != Program::no_id) {
		glUniformMatrix4fv(mvp_attribute, 1, GL_FALSE, &mvp[0][0]);
	//	check("SceneObject::draw set mvp");
	}

	glDrawElements(GL_TRIANGLES, (elems_count + 3) * 3, GL_UNSIGNED_INT, nullptr);
//	check("SceneObject::draw draw");

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void SceneObject::set_attribute_to_position(GLuint attribute) const {
	glBindVertexArray(vao);
//	check("SceneObject::set_pos bind vao");
	glBindBuffer(GL_ARRAY_BUFFER, data);
//	check("SceneObject::set_pos bind data");
	
	glEnableVertexAttribArray(attribute);
//	check("SceneObject::set_pos enable");
	glVertexAttribPointer(attribute,
		3, GL_FLOAT,
		GL_FALSE,
		sizeof(Object::vertex_data), reinterpret_cast<GLvoid const *>(offsetof(Object::vertex_data, pos))
	);
//	check("SceneObject::set_pos attibute");
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void SceneObject::set_attribute_to_normal(GLuint attribute) const {
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

void SceneObject::set_attribute_to_uv(GLuint attribute) const {
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, data);

	glEnableVertexAttribArray(attribute);
	glVertexAttribPointer(attribute,
		2, GL_FLOAT,
		GL_FALSE,
		sizeof(Object::vertex_data), reinterpret_cast<GLvoid const *>(offsetof(Object::vertex_data, uv))
	);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}
