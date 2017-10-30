#include "hw2_window.hpp"
#include "hw2_error.hpp"

#include <epoxy/gl.h>

#include <glm/gtc/matrix_transform.hpp>
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

	auto load_resource{[this](std::string const &path) -> std::tuple<char const *, size_t> {
		auto resource_bytes{Resource::lookup_data_global(path)};
		gsize resource_size;
		auto resource{static_cast<const char*>(resource_bytes->get_data(resource_size))};
		return {resource, resource_size};
	}};

	/* base */ {
		glEnable(GL_DEPTH_TEST);
	}

	/* shaders */ {
		auto create_shader{[&,this](GLenum type, std::string const &path) -> GLuint {
			char const *source;
			size_t source_size;
			std::tie(source, source_size) = load_resource(path);

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

	/* scene */ {
		::Object obj{::Object::load(std::get<0>(load_resource("/net/ldvsoft/spbau/gl/stanford_bunny.obj")))};
		obj.recalculate_normals();
		std::cout << obj << std::endl;
		gl.object = std::make_unique<SceneObject>(obj);
		gl.object->position = glm::scale(glm::vec3(10.0f));

		/* position */ {
			gl.object->set_attribute_to_position(gl.shader.position_location);
		}
		/* color */ {
			gl.object->set_attribute_to_normal(gl.shader.color_location);
		}

		update_camera();
	}
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
	gl.perspective = glm::perspective(
		/* vertical fov = */ glm::radians(60.0f),
		/* ratio        = */ static_cast<float>(area->get_width()) / area->get_height(),
		/* planes       : */ 1.0f, 10000.0f
	);

	glClearColor(.3, .3, .3, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(gl.shader.program);

	gl.object->draw(gl.shader.mvp_location, gl.perspective * gl.camera);

	glUseProgram(0);

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
		animation.state = animation.PENDING;
		animation.id = gtk_widget_add_tick_callback(GTK_WIDGET(area->gobj()), animate_tick_wrapper, this, nullptr);
	} else {
		animation.state = animation.STOPPED;
		gtk_widget_remove_tick_callback(GTK_WIDGET(area->gobj()), animation.id);
	}
}

void Hw2Window::animate_tick(gint64 new_time) {
	if (animation.state == animation.PENDING) {
		animation.start_time = new_time;
		animation.start_angle = gl.angle;
		animation.state = animation.STARTED;
		return;
	}

	gint64 delta{new_time - animation.start_time};
	double secondsDelta{delta / static_cast<double>(G_USEC_PER_SEC)};
	gl.angle = fmod(animation.start_angle + secondsDelta * animation.angle_per_second, 2 * M_PI);
	update_camera();
	area->queue_render();
}

void Hw2Window::update_camera() {
	double t{cos(3 * gl.angle) * M_PI / 6};
	float const r{4};
	gl.camera = glm::lookAt(
		/* from = */ glm::vec3(
			r * sin(gl.angle) * cos(t),
			r * sin(t),
			r * cos(gl.angle) * cos(t)
		),
		/* to   = */ glm::vec3(0.0f, 0.0f, 0.0f),
		/* up   = */ glm::vec3(0.0f, 1.0f, 0.0f)
	);
}
