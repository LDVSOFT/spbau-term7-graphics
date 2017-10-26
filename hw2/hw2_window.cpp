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

[[maybe_unused]]
static void check(std::string const &name) {
	GLuint error{glGetError()};
	if (error == GL_NO_ERROR)
		std::cout << "Check " << name << " ok" << std::endl;
	else
		std::cout << "Check " << name << " FAILED " << std::hex << error << std::endl;
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
	builder->get_widget("reset_button", reset);

	area->set_has_depth_buffer();

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

	/* base */ {
		glEnable(GL_DEPTH_TEST);
	}

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
			gl.shader.position_location = glGetAttribLocation (gl.shader.program, "vertex_position");
			gl.shader.color_location    = glGetAttribLocation (gl.shader.program, "vertex_color");

			gl.shader.mvp_location      = glGetUniformLocation(gl.shader.program, "mvp");
		}
	}

	/* object */ {
		glGenVertexArrays(1, &gl.scene.vao);
		glBindVertexArray(gl.scene.vao);

		static const struct vertex_data_s { GLfloat pos[3]; GLfloat clr[3]; } vertex_data[] = {
			{ { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f, 0.0f } },
			{ { -1.0f, -1.0f, +1.0f }, { 0.0f, 0.0f, 1.0f } },
			{ { +1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f, 0.0f } },
			{ { +1.0f, -1.0f, +1.0f }, { 1.0f, 0.0f, 1.0f } },
			{ { -1.0f, +1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f } },
			{ { -1.0f, +1.0f, +1.0f }, { 0.0f, 1.0f, 1.0f } },
			{ { +1.0f, +1.0f, -1.0f }, { 1.0f, 1.0f, 0.0f } },
			{ { +1.0f, +1.0f, +1.0f }, { 1.0f, 1.0f, 1.0f } },
		};
		static const GLushort vertex_ids[12][3] = {
			{ 0, 1, 2 }, { 1, 2, 3 },
			{ 0, 1, 4 }, { 1, 4, 5 },
			{ 0, 2, 4 }, { 2, 4, 6 },
			{ 1, 3, 5 }, { 3, 5, 7 },
			{ 2, 3, 6 }, { 3, 6, 7 },
			{ 4, 5, 6 }, { 5, 6, 7 },
		};

		GLuint vertex_data_buffer;
		glGenBuffers(1, &vertex_data_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, vertex_data_buffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);

		glGenBuffers(1, &gl.scene.elements_buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl.scene.elements_buffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(vertex_ids), vertex_ids, GL_STATIC_DRAW);

		/* position */ {
			glEnableVertexAttribArray(gl.shader.position_location);
			glVertexAttribPointer(gl.shader.position_location,
					3, GL_FLOAT,
					GL_FALSE,
					sizeof(vertex_data_s), reinterpret_cast<GLvoid const *>(offsetof(vertex_data_s, pos))
			);
		}
		/* color */ {
			glEnableVertexAttribArray(gl.shader.color_location);
			glVertexAttribPointer(gl.shader.color_location,
					3, GL_FLOAT,
					GL_FALSE,
					sizeof(vertex_data_s), reinterpret_cast<GLvoid const *>(offsetof(vertex_data_s, clr))
			);
		}

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		glDeleteBuffers(1, &vertex_data_buffer);
	}

	update_camera();
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

	glClearColor(.3, .3, .3, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (gl.shader.program != 0 && gl.scene.vao != 0) {
		gl.scene.mvp = glm::perspective(
			/* vertical fov = */ glm::radians(60.0f),
			/* ratio        = */ static_cast<float>(area->get_width()) / area->get_height(),
			/* planes       : */ 1.0f, 10000.0f
		) * gl.scene.projection * glm::mat4(1.0f);

		glUseProgram(gl.shader.program);
		glBindVertexArray(gl.scene.vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl.scene.elements_buffer);

		glUniformMatrix4fv(gl.shader.mvp_location, 1, GL_FALSE, &gl.scene.mvp[0][0]);

		glDrawElements(GL_TRIANGLES, 6 * 2 * 3, GL_UNSIGNED_SHORT, nullptr);

		glUseProgram(0);
		glBindVertexArray(0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
	glFlush();

	return false;
}

static gboolean animate_tick_wrapper(GtkWidget *, GdkFrameClock *clock, gpointer data) {
	static_cast<Hw2Window *>(data)->animate_tick(gdk_frame_clock_get_frame_time(clock));
	return G_SOURCE_CONTINUE;
}

void Hw2Window::animate_toggled() {
	bool state{animate->get_active()};
	if (state) {
		animation_state = PENDING;
		animation_id = gtk_widget_add_tick_callback(GTK_WIDGET(area->gobj()), animate_tick_wrapper, this, nullptr);
	} else {
		animation_state = STOPPED;
		gtk_widget_remove_tick_callback(GTK_WIDGET(area->gobj()), animation_id);
	}
}

void Hw2Window::animate_tick(gint64 new_time) {
	if (animation_state == PENDING) {
		animation_start_time = new_time;
		animation_start_angle = gl.scene.angle;
		animation_state = STARTED;
		return;
	}

	gint64 delta{new_time - animation_start_time};
	double secondsDelta{delta / static_cast<double>(G_USEC_PER_SEC)};
	gl.scene.angle = fmod(animation_start_angle + secondsDelta * anglePerSecond, 2 * M_PI);
	update_camera();
	area->queue_render();
}

void Hw2Window::update_camera() {
	double t{cos(3 * gl.scene.angle) * M_PI / 6};
	gl.scene.projection = glm::lookAt(
		/* from = */ glm::vec3(
			4 * sin(gl.scene.angle) * cos(t),
			4 * sin(t),
			4 * cos(gl.scene.angle) * cos(t)
		),
		/* to   = */ glm::vec3(0.0f, 0.0f, 0.0f),
		/* up   = */ glm::vec3(0.0f, 1.0f, 0.0f)
	);
}
