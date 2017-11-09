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
	builder->get_widget("display_mode_combobox", display_mode_combobox);
	builder->get_widget("reset_button", reset);

	display_mode_list_store = RefPtr<Gtk::ListStore>::cast_dynamic(builder->get_object("display_mode_list_store"));

	area->set_has_depth_buffer();
	/* options */ {
		/* scene */ {
			static_assert(SCENE == 0);
			auto &row{*display_mode_list_store->append()};
			row.set_value<Glib::ustring>(0, "Scene");
		}
		/* shadowmap */ {
			static_assert(SHADOWMAP == 1);
			auto &row{*display_mode_list_store->append()};
			row.set_value<Glib::ustring>(0, "Shadowmap");
		}

		display_mode_combobox->set_active(SCENE);
	}

	area->signal_realize().connect(sigc::mem_fun(*this, &Hw2Window::gl_init));
	area->signal_unrealize().connect(sigc::mem_fun(*this, &Hw2Window::gl_finit), false);
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
		std::string vertex(std::get<0>(load_resource("/net/ldvsoft/spbau/gl/hw2_vertex.glsl")));
		std::string fragment(std::get<0>(load_resource("/net/ldvsoft/spbau/gl/hw2_fragment.glsl")));
		std::string error_string;
		gl.program = Program::build_program({{GL_VERTEX_SHADER, vertex}, {GL_FRAGMENT_SHADER, fragment}}, error_string);
		if (gl.program == nullptr) {
			Error error(hw2_error_quark, 0, error_string);
			area->set_error(error);

			return;
		}

		/* parameters */ {
			gl.locations.vertex_position = gl.program->get_attribute("vertex_position_model");
			gl.locations.vertex_normal   = gl.program->get_attribute("vertex_normal_model");

			gl.locations.m               = gl.program->get_uniform  ("m");
			gl.locations.v               = gl.program->get_uniform  ("v");
			gl.locations.mv              = gl.program->get_uniform  ("mv");
			gl.locations.mvp             = gl.program->get_uniform  ("mvp");

			gl.locations.light_position  = gl.program->get_uniform  ("light_world");
			gl.locations.light_color     = gl.program->get_uniform  ("light_color");
			gl.locations.light_power     = gl.program->get_uniform  ("light_power");
		}
	}

	/* framebuffer */ {
		glGenFramebuffers(1, &gl.framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, gl.framebuffer);

		GLuint depth_texture;
		glGenTextures(1, &depth_texture);
		glBindTexture(GL_TEXTURE_2D, depth_texture);
		glTexImage2D(
			GL_TEXTURE_2D, /* mipmap_level = */ 0,
			GL_DEPTH_COMPONENT16,
			gl.depth_buffer_size, gl.depth_buffer_size, 0,
			GL_DEPTH_COMPONENT, GL_FLOAT,
			nullptr
		);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth_texture, /* mipmap_level = */ 0);
		glDrawBuffer(GL_NONE);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			area->set_error(Error(hw2_error_quark, 0, "Failed to create framebuffer."));
			glDeleteFramebuffers(1, &gl.framebuffer);
			return;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	/* scene */ {
		::Object obj{::Object::load(std::get<0>(load_resource("/net/ldvsoft/spbau/gl/stanford_bunny.obj")))};
		obj.recalculate_normals();
		gl.object = std::make_unique<SceneObject>(obj);
		//gl.object->position = glm::scale(glm::vec3(10.0f));

		/* position */ {
			gl.object->set_attribute_to_position(gl.locations.vertex_position);
//			check("Hw2Window::gl_init set position attribute");
		}
		/* color */ {
			gl.object->set_attribute_to_normal(gl.locations.vertex_normal);
//			check("Hw2Window::gl_init set normal attribute");
		}

		update_camera();
	}

	std::cout << "Rendering on " << glGetString(GL_RENDERER) << std::endl;
}

void Hw2Window::gl_finit() {
	area->make_current();
	if (area->has_error())
		return;
	gl.object = nullptr;
	gl.program = nullptr;
}

bool Hw2Window::gl_render(RefPtr<GLContext> const &context) {
	(void) context;
//	check("Hw2Window::gl_render before...");

	gl.perspective = glm::perspective(
		/* vertical fov = */ glm::radians(60.0f),
		/* ratio        = */ static_cast<float>(area->get_width()) / area->get_height(),
		/* planes       : */ .01f, 1000.0f
	);

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	gl.program->use();
//	check("Hw2Window::gl_render use program");

	glUniform3f(gl.locations.light_color, 1.0f, 1.0f, 1.0f);
//	check("Hw2Window::gl_render set light color");
	glUniform3f(gl.locations.light_position, 3.0f, 3.0f, 3.0f);
//	check("Hw2Window::gl_render set light position");
	glUniform1f(gl.locations.light_power, 15.0f);
//	check("Hw2Window::gl_render set light power");

	gl.object->draw(
		gl.camera, gl.perspective,
		gl.locations.m, gl.locations.v, /* p_attribute = */ SceneObject::no_attribute,
		gl.locations.mv, gl.locations.mvp
	);

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
	float const r{.4};
	gl.camera = glm::lookAt(
		/* from = */ glm::vec3(
			r * sin(gl.angle) * cos(t),
			r * sin(t) + 0.1,
			r * cos(gl.angle) * cos(t)
		),
		/* to   = */ glm::vec3(0.0f, 0.1f, 0.0f),
		/* up   = */ glm::vec3(0.0f, 1.0f, 0.0f)
	);
}
