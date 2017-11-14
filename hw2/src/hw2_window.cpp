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
		/* scene_from_sum */ {
			static_assert(SCENE_FROM_SUN == 1);
			auto &row{*display_mode_list_store->append()};
			row.set_value<Glib::ustring>(0, "Scene (from sun)");
		}
		/* shadowmap */ {
			static_assert(SHADOWMAP == 2);
			auto &row{*display_mode_list_store->append()};
			row.set_value<Glib::ustring>(0, "Shadowmap");
		}

		display_mode_combobox->set_active(SCENE);
	}

	area->signal_realize().connect(sigc::mem_fun(*this, &Hw2Window::gl_init));
	area->signal_unrealize().connect(sigc::mem_fun(*this, &Hw2Window::gl_finit), false);
	area->signal_render().connect(sigc::mem_fun(*this, &Hw2Window::gl_render));

	animate->signal_toggled().connect(sigc::mem_fun(*this, &Hw2Window::animate_toggled));
	reset->signal_clicked().connect(sigc::mem_fun(*this, &Hw2Window::reset_clicked));
	display_mode_combobox->signal_changed().connect(sigc::mem_fun(*this, &Hw2Window::display_mode_changed));

	gl.light_position = glm::vec3(0, .1, .15);
	gl.light_color = glm::vec3(1, 1, 1);
	gl.light_power = .006;

	gl.sun_position = glm::vec3(-.1, .1, -.1);
	gl.sun_color = glm::vec3(1, .6, 0);
	gl.sun_power = .9;
	gl.sun_view_range = .3;
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

	auto report_error{[this](std::string const &msg) -> void {
		Error error(hw2_error_quark, 0, msg);
		area->set_error(error);
		std::cout << "ERROR: " << msg << std::endl;
	}};

	/* shaders */ {
		/* scene */ {
			std::string vertex(std::get<0>(load_resource("/net/ldvsoft/spbau/gl/scene_vertex.glsl")));
			std::string fragment(std::get<0>(load_resource("/net/ldvsoft/spbau/gl/scene_fragment.glsl")));
			std::string error_string;
			gl.scene_program = Program::build_program({{GL_VERTEX_SHADER, vertex}, {GL_FRAGMENT_SHADER, fragment}}, error_string);
			if (gl.scene_program == nullptr) {
				report_error("Program Scene: " + error_string);
				return;
			}
		}

		/* shadowmap */ {
			std::string vertex(std::get<0>(load_resource("/net/ldvsoft/spbau/gl/shadowmap_vertex.glsl")));
			std::string fragment(std::get<0>(load_resource("/net/ldvsoft/spbau/gl/shadowmap_fragment.glsl")));
			std::string error_string;
			gl.shadowmap_program = Program::build_program({{GL_VERTEX_SHADER, vertex}, {GL_FRAGMENT_SHADER, fragment}}, error_string);
			if (gl.shadowmap_program == nullptr) {
				report_error("Program Shadowmap: " + error_string);
				return;
			}
		}
	}

	/* framebuffer */ {
		glGenFramebuffers(1, &gl.framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, gl.framebuffer);

		glGenTextures(1, &gl.shadowmap);
		glBindTexture(GL_TEXTURE_2D, gl.shadowmap);
		glTexImage2D(
			GL_TEXTURE_2D, /* mipmap_level = */ 0,
			GL_DEPTH_COMPONENT16,
			gl.shadowmap_size, gl.shadowmap_size, 0,
			GL_DEPTH_COMPONENT, GL_FLOAT,
			nullptr
		);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// FIXME
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, gl.shadowmap, /* mipmap_level = */ 0);
		glDrawBuffer(GL_NONE);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			area->set_error(Error(hw2_error_quark, 0, "Failed to create framebuffer."));
			glDeleteFramebuffers(1, &gl.framebuffer);
			glDeleteTextures(1, &gl.shadowmap);
			return;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	/* scene */ {
		/* rabbit */ {
			::Object obj{::Object::load(std::get<0>(load_resource("/net/ldvsoft/spbau/gl/stanford_bunny.obj")))};
			obj.recalculate_normals();
			gl.object = std::make_unique<SceneObject>(obj);
			gl.object->position = glm::translate(glm::mat4(1), glm::vec3(0, -.03, 0));
		}

		/* base plane */ {
			::Object obj{::Object::load(std::get<0>(load_resource("/net/ldvsoft/spbau/gl/plane.obj")))};
			std::cout << obj << std::endl;
			gl.base_plane = std::make_unique<SceneObject>(obj);
		}
		gl.sun_proj = glm::ortho<float>(
			/* X ragne : */ -gl.sun_view_range, +gl.sun_view_range,
			/* Y ragne : */ -gl.sun_view_range, +gl.sun_view_range,
			/* planes  : */ -10, 10
		);
		gl.sun_view = glm::lookAt(
			/* from = */ gl.sun_position,
			/* to   = */ glm::vec3(0, 0, 0),
			/* up   = */ glm::vec3(0, 1, 0)
		);
	}

	std::cout << "Rendering on " << glGetString(GL_RENDERER) << std::endl;
}

void Hw2Window::gl_finit() {
	area->make_current();
	if (area->has_error())
		return;
	gl.object = nullptr;
	gl.base_plane = nullptr;
	gl.scene_program = nullptr;
	gl.shadowmap_program = nullptr;
}

bool Hw2Window::gl_render(RefPtr<GLContext> const &context) {
	static_cast<void>(context);
//	check("Hw2Window::gl_render before...");

	if (area->has_error())
		return false;

	glClearColor(0, 0, 0, 1);

	/* animate the light */ {
	}

	/* shadowmap */ {
		GLint old_buffer, old_vp[4];
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_buffer);
		glGetIntegerv(GL_VIEWPORT, old_vp);

		glBindFramebuffer(GL_FRAMEBUFFER, gl.framebuffer);
		glViewport(0, 0, gl.shadowmap_size, gl.shadowmap_size);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		gl_render_shadowmap();

		glBindFramebuffer(GL_FRAMEBUFFER, old_buffer);
		glViewport(old_vp[0], old_vp[1], old_vp[2], old_vp[3]);
	}
	glBindTexture(GL_TEXTURE_2D, gl.shadowmap);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	switch (display_mode_combobox->get_active_row_number()) {
	case SCENE:
		gl_render_scene();
		break;
	case SCENE_FROM_SUN:
		gl_render_scene_from_sun();
		break;
	case SHADOWMAP:
		gl_render_shadowmap();
		break;
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	glFlush();

	return false;
}

void Hw2Window::gl_render_scene() {
	gl.scene_program->use();

	//glm::mat4 bias{glm::translate(glm::mat4(.5), glm::vec3(.5, .5, .5))};
//	glm::mat4 bias(
//		.5,  0,  0, 0,
//		 0, .5,  0, 0,
//		 0,  0, .5, 0,
//		 5, .5, .5, 1
//	);;
	glm::mat4 shadowmap_vp{gl.sun_proj * gl.sun_view};

	glUniform3fv(gl.scene_program->get_uniform("light_world"), 1, &gl.light_position[0]);
	glUniform3fv(gl.scene_program->get_uniform("light_color"), 1, &gl.light_color[0]);
	glUniform1f (gl.scene_program->get_uniform("light_power"), gl.light_power);

	glUniform3fv(gl.scene_program->get_uniform("tosun_world"), 1, &gl.sun_position[0]);
	glUniform3fv(gl.scene_program->get_uniform("sun_color"  ), 1, &gl.sun_color[0]);
	glUniform1f (gl.scene_program->get_uniform("sun_power"), gl.sun_power);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gl.shadowmap);
	glUniform1i (gl.scene_program->get_uniform("shadowmap"), 0 /*gl.shadowmap*/);
	glUniformMatrix4fv(
		gl.scene_program->get_uniform("shadowmap_vp"),
		1, GL_FALSE,
		&shadowmap_vp[0][0]
	);

	double t{cos(3 * gl.angle) * M_PI / 12 + M_PI / 8};
	float const r{.4};
	glm::mat4 cam_view{glm::lookAt(
		/* from = */ glm::vec3(
			r * sin(gl.angle) * cos(t),
			r * sin(t) + 0.1,
			r * cos(gl.angle) * cos(t)
		),
		/* to   = */ glm::vec3(0.0f, 0.1f, 0.0f),
		/* up   = */ glm::vec3(0.0f, 1.0f, 0.0f)
	)};
	glm::mat4 cam_proj{glm::perspective(
		/* vertical fov = */ glm::radians(60.0f),
		/* ratio        = */ static_cast<float>(area->get_width()) / area->get_height(),
		/* planes       : */ .01f, 1000.0f
	)};
	gl_draw_objects(*gl.scene_program, cam_view, cam_proj);

	glUseProgram(1);
}

void Hw2Window::gl_render_scene_from_sun() {
	gl.scene_program->use();

	glUniform3fv(gl.scene_program->get_uniform("light_world"), 1, &gl.light_position[0]);
	glUniform3fv(gl.scene_program->get_uniform("light_color"), 1, &gl.light_color[0]);
	glUniform1f (gl.scene_program->get_uniform("light_power"), gl.light_power);

	glUniform3fv(gl.scene_program->get_uniform("tosun_world"), 1, &gl.sun_position[0]);
	glUniform3fv(gl.scene_program->get_uniform("sun_color"  ), 1, &gl.sun_color[0]);
	glUniform1f (gl.scene_program->get_uniform("sun_power"), gl.sun_power);

	gl_draw_objects(*gl.scene_program, gl.sun_view, gl.sun_proj);

	glUseProgram(0);
}

void Hw2Window::gl_render_shadowmap() {
	gl.shadowmap_program->use();

	gl_draw_objects(*gl.shadowmap_program, gl.sun_view, gl.sun_proj);

	glUseProgram(0);
}

void Hw2Window::gl_draw_objects(Program const &program, glm::mat4 const &v, glm::mat4 const &p) {
	for (SceneObject const *object: {gl.object.get(), gl.base_plane.get()}) {
		auto pos{program.get_attribute("vertex_position_model")};
		auto nor{program.get_attribute("vertex_normal_model")};
		if (pos != Program::no_id)
			object->set_attribute_to_position(pos);
		if (nor != Program::no_id)
			object->set_attribute_to_normal(nor);
		object->draw(
			v, p,
			program.get_uniform("m"), program.get_uniform("v"), program.get_uniform("p"),
			program.get_uniform("mv"), program.get_uniform("mvp")
		);
	}
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

void Hw2Window::reset_clicked() {
	gl.angle = 0;
	if (animation.state == animation.STARTED)
		animation.state = animation.PENDING;
	area->queue_render();
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
	area->queue_render();
}

void Hw2Window::display_mode_changed() {
	area->queue_render();
}
