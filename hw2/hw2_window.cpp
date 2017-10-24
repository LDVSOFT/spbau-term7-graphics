#include "hw2_window.hpp"
#include "hw2_error.hpp"

#include <epoxy/gl.h>

#include <glm/gtx/transform.hpp>

#include <iostream>

using Gdk::GLContext;
using Gio::Resource;
using Glib::Bytes;
using Glib::Error;
using Glib::RefPtr;
using Glib::ustring;
using Gtk::Application;
using Gtk::Builder;
using Gtk::Window;
using std::unique_ptr;

unique_ptr<Hw2Window> Hw2Window::create() {
	auto builder{Gtk::Builder::create_from_resource("/net/ldvsoft/spbau/gl/hw2_window.ui")};
	Hw2Window *result;
	builder->get_widget_derived("Hw2Window", result);
	
	return unique_ptr<Hw2Window>(result);
}

Hw2Window::Hw2Window(
	BaseObjectType *type,
	RefPtr<Builder> const &builder
):
	Window(type),
	builder(builder)
{
	builder->get_widget("draw_area", area);
	builder->get_widget("draw_area_eventbox", area_eventbox);
	builder->get_widget("animate_toggle", animate);

	area->signal_realize().connect(sigc::mem_fun(*this, &Hw2Window::gl_init));
	area->signal_unrealize().connect(sigc::mem_fun(*this, &Hw2Window::gl_finit));
	area->signal_render().connect(sigc::mem_fun(*this, &Hw2Window::gl_render));

	animate->signal_toggled().connect(sigc::mem_fun(*this, &Hw2Window::animate_toggled));
}

Hw2Window::~Hw2Window() = default;

void Hw2Window::gl_init() {
	area->make_current();
	if (area->has_error())
		return;

	/* shaders */ {
		auto create_shader{[this](GLenum type, std::string const &path) -> GLuint {
			auto source_resource{Resource::lookup_data_global(path)};
			gsize source_size;
			auto source{static_cast<const char*>(source_resource->get_data(source_size))};
			
			GLuint shader{glCreateShader(type)};
			/* meh */ {
				auto size{static_cast<GLint>(source_size)};
				glShaderSource(shader, 1, &source, &size);
			}
			glCompileShader(shader);

			GLint status;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
			if (status == GL_FALSE) {
				GLint length;
				glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
				std::string error_string(length + 1, '\0');
				glGetShaderInfoLog(shader, length, nullptr, &error_string[0]);

				Error error(hw2_error_quark, 0, error_string);
				area->set_error(error);

				glDeleteShader(shader);
				return 0;
			}

			return shader;
		}};

		GLuint vertex{create_shader(GL_VERTEX_SHADER, "/net/ldvsoft/spbau/gl/hw2_vertex.glsl")};
		if (vertex == 0)
			return;
		GLuint fragment{create_shader(GL_FRAGMENT_SHADER, "/net/ldvsoft/spbau/gl/hw2_fragment.glsl")};
		if (fragment == 0) {
			glDeleteShader(vertex);
			return;
		}

		gl.shader.program = glCreateProgram();
		if (gl.shader.program == 0)
			return;
		glAttachShader(gl.shader.program, vertex);
		glAttachShader(gl.shader.program, fragment);
		glLinkProgram(gl.shader.program);
		glDetachShader(gl.shader.program, fragment);
		glDeleteShader(fragment);
		glDetachShader(gl.shader.program, vertex);
		glDeleteShader(vertex);

		GLint status;
		glGetProgramiv(gl.shader.program, GL_LINK_STATUS, &status);
		if (status == GL_FALSE) {
			GLint length;
			glGetProgramiv(gl.shader.program, GL_INFO_LOG_LENGTH, &length);
			std::string error_string(length + 1, '\0');
			glGetProgramInfoLog(gl.shader.program, length, nullptr, &error_string[0]);

			Error error(hw2_error_quark, 0, error_string);
			area->set_error(error);

			glDeleteProgram(gl.shader.program);
			gl.shader.program = 0;
			return;
		}

		/* parameters */ {
			gl.shader.position_location = glGetAttribLocation (gl.shader.program, "position");

			gl.shader.mvp_location      = glGetUniformLocation(gl.shader.program, "mvp");
		}
	}

	/* object */ {
		glGenVertexArrays(1, &gl.scene.vao);
		glBindVertexArray(gl.scene.vao);

		static const GLfloat vertex_data[4][2] = {
			{  0.5f,  0.5f },
			{ -0.5f,  0.5f },
			{ -0.5f, -0.5f },
			{  0.5f, -0.5f },
		};

		GLuint buffer;
		glGenBuffers(1, &buffer);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);

		/* position */ {
			glEnableVertexAttribArray(gl.shader.position_location);
			glVertexAttribPointer(gl.shader.position_location, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat[2]), nullptr);
		}

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		glDeleteBuffers(1, &buffer);
	}

	gl.scene.projection = glm::lookAt(glm::vec3(2.0f,2.0f,2.0f),glm::vec3(0.0f,0.0f,0.0f),glm::vec3(0.0f,1.0f,0.0f));

	std::cout << "GL init" << std::endl;
}

void Hw2Window::gl_finit() {
	std::cout << "GL finit" << std::endl;
	area->make_current();
	if (area->has_error())
		return;
	std::cout << "GL actual finit" << std::endl;
}

bool Hw2Window::gl_render(RefPtr<GLContext> const &context) {
	(void) context;

	std::cout << "GL render" << std::endl;

	glClearColor(.0, .0, .0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	if (gl.shader.program != 0 && gl.scene.vao != 0) {
		gl.scene.mvp = glm::perspective(glm::radians(60.0f), static_cast<float>(area->get_width()) / area->get_height(), 0.1f, 100.0f) * gl.scene.projection;

		glUseProgram(gl.shader.program);
		glBindVertexArray(gl.scene.vao);

		glUniformMatrix4fv(gl.shader.mvp_location, 1, GL_FALSE, &gl.scene.mvp[0][0]);

		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		glUseProgram(0);
		glBindVertexArray(0);
	}
	glFlush();

	return false;
}

void Hw2Window::animate_toggled() {
	bool state{animate->get_active()};
	if (state) {
	} else {
	}
}
