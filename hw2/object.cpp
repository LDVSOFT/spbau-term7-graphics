#include "object.hpp"

#include <glm/gtx/vector_angle.hpp>

#include <cstdio>
#include <sstream>
#include <iostream>
#include <map>

static std::vector<std::string> split_by(std::string const &s, char c) {
	std::istringstream stream(s);
	std::string elem;
	std::vector<std::string> result;
	while (std::getline(stream, elem, c))
		result.push_back(elem);
	return result;
}

object object::load(std::string const &obj) {
	std::vector<glm::vec3> v;
	std::vector<glm::vec3> vn;
	std::vector<glm::vec2> vt;

	std::map<std::tuple<size_t, size_t, size_t>, size_t> v_ids;

	object result;

	for (std::string const &line: split_by(obj, '\n')) {
		if (line.empty() || line[0] == '#')
			continue;
		auto tokens(split_by(line, ' '));
		auto type(tokens[0]);
		tokens.erase(tokens.begin());

		if (type == "v") {
			v.push_back(glm::vec3(stof(tokens[0]), stof(tokens[1]), stof(tokens[2])));
		} else if (type == "vn") {
			vn.push_back(glm::vec3(stof(tokens[0]), stof(tokens[1]), stof(tokens[2])));
		} else if (type == "vt") {
			vt.push_back(glm::vec2(stof(tokens[0]), stof(tokens[1])));
		} else if (type == "f") {
			std::vector<size_t> verts;
			for (auto const &t: tokens) {
				auto elems(split_by(t, '/'));
				size_t constexpr bad(-1);

				size_t v_id, vt_id(bad), vn_id(bad);
				v_id = stoull(elems[0]) - 1;
				if (elems.size() >= 2 && !elems[1].empty())
					vt_id = stoull(elems[1]) - 1;
				if (elems.size() >= 3 && !elems[2].empty())
					vn_id = stoull(elems[2]) - 1;

				std::tuple<size_t, size_t, size_t> id{v_id, vt_id, vn_id};
				if (v_ids.count(id) == 0) {
					vertex_data vertex;
					vertex.pos  = v[v_id];
					vertex.norm = (vn_id == bad) ? glm::vec3{0, 0, 0} : v[vn_id];
					vertex.uv   = (vt_id == bad) ? glm::vec2{0, 0}    : v[vt_id];
					result.verticies.push_back(vertex);
					v_ids[id] = result.verticies.size() - 1;
				}
				verts.push_back(v_ids[id]);
			}
			for (size_t i{1}; i + 1 < verts.size(); ++i) {
				result.faces.push_back({verts[0], verts[i], verts[i + 1]});
			}
		} else {
			std::cout << "UNKNOWN TYPE " << type << std::endl;
		}
	}
	return result;
}

void object::recalculate_normals() {
	std::vector<glm::vec3> new_normals(verticies.size());
	for (size_t id{0}; id != faces.size(); ++id) {
		size_t const i{faces[id][0]};
		size_t const j{faces[id][1]};
		size_t const k{faces[id][2]};
		auto const &a{verticies[i].pos};
		auto const &b{verticies[j].pos};
		auto const &c{verticies[k].pos};
		glm::vec3 norm{glm::normalize(glm::cross(b - a, c - a))};

		new_normals[i] += norm * angle(normalize(b - a), normalize(c - a));
		new_normals[j] += norm * angle(normalize(a - b), normalize(c - b));
		new_normals[k] += norm * angle(normalize(a - c), normalize(b - c));
	}
	for (size_t i{0}; i != verticies.size(); ++i)
		verticies[i].norm = glm::normalize(new_normals[i]);
}

std::ostream &operator<<(std::ostream &s, object const &o) {
	s << "object: " << o.verticies.size() << " verticies, " << o.faces.size() << " faces.\n";
	for (size_t i{0}; i != o.verticies.size(); ++i) {
		auto const &v{o.verticies[i]};
		s << "v" << i << ": ";
		s << "pos=" << v.pos.x << "," << v.pos.y << "," << v.pos.z << " ";
		s << "norm=" << v.norm.x << "," << v.norm.y << "," << v.norm.z << " ";
		s << "uv=" << v.uv.x << "," << v.uv.y << "\n";
	}
	for (size_t i{0}; i != o.faces.size(); ++i) {
		s << "f" << i << ": ";
		s << o.faces[i][0] << " " << o.faces[i][1] << " " << o.faces[i][2];
		s << '\n';
	}
	return s;
}
