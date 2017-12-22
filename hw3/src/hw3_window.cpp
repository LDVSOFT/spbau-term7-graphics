#include "hw3_window.hpp"
#include "hw3_error.hpp"

#include <epoxy/gl.h>

#include <gdk/gdkkeysyms.h>

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
	auto builder{Gtk::Builder::create_from_resource("/net/ldvsoft/spbau/gl/hw3_window.ui")};
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

static gboolean tick_wrapper(GtkWidget *, GdkFrameClock *clock, gpointer data) {
	static_cast<Hw2Window *>(data)->tick(gdk_frame_clock_get_frame_time(clock));
	return G_SOURCE_CONTINUE;
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
	builder->get_widget("reset_position_button", reset_position);
	builder->get_widget("reset_animation_button", reset_animation);

	ticker_id = gtk_widget_add_tick_callback(GTK_WIDGET(area->gobj()), tick_wrapper, this, nullptr);

	display_mode_list_store = RefPtr<Gtk::ListStore>::cast_dynamic(builder->get_object("display_mode_list_store"));
	lights_adjustment = RefPtr<Gtk::Adjustment>::cast_dynamic(builder->get_object("lights_adjustment"));

	area->set_has_depth_buffer();
	/* options */ {
		/* scene */ {
			static_assert(SCENE == 0);
			auto &row{*display_mode_list_store->append()};
			row.set_value<Glib::ustring>(0, "Scene");
		}
		/* deferred: lights */ {
			static_assert(DEFERRED_LIGHTS == 1);
			auto &row{*display_mode_list_store->append()};
			row.set_value<Glib::ustring>(0, "Deferred: lights");
		}

		display_mode_combobox->set_active(SCENE);
	}

	area->signal_realize  ().connect(sigc::mem_fun(*this, &Hw2Window::gl_init));
	area->signal_unrealize().connect(sigc::mem_fun(*this, &Hw2Window::gl_finit), false);
	area->signal_render   ().connect(sigc::mem_fun(*this, &Hw2Window::gl_render));

	area_eventbox->signal_button_press_event  ().connect(sigc::mem_fun(*this, &Hw2Window::mouse_pressed));
	area_eventbox->signal_button_release_event().connect(sigc::mem_fun(*this, &Hw2Window::mouse_pressed));
	area_eventbox->signal_motion_notify_event ().connect(sigc::mem_fun(*this, &Hw2Window::mouse_moved));

	signal_key_press_event  ().connect(sigc::mem_fun(*this, &Hw2Window::key_pressed));
	signal_key_release_event().connect(sigc::mem_fun(*this, &Hw2Window::key_released));

	animate->signal_toggled().connect(sigc::mem_fun(*this, &Hw2Window::animate_toggled));
	reset_position->signal_clicked ().connect(sigc::mem_fun(*this, &Hw2Window::reset_position_clicked));
	reset_animation->signal_clicked().connect(sigc::mem_fun(*this, &Hw2Window::reset_animation_clicked));
	display_mode_combobox->signal_changed().connect(sigc::mem_fun(*this, &Hw2Window::display_mode_changed));

	gl.lights.resize(1);
	gl.lights[0].position = glm::vec3(0, .1, .5);
	gl.lights[0].color = glm::vec3(1, 1, 1);
	gl.lights[0].power = .04;
	gl.lights[0].radius = .1;

	view_range = .3;

	reset_position_clicked();
	reset_animation_clicked();
}

Hw2Window::~Hw2Window() {
	gtk_widget_remove_tick_callback(GTK_WIDGET(area->gobj()), ticker_id);
}

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
		Error error(hw3_error_quark, 0, msg);
		area->set_error(error);
		std::cout << "ERROR: " << msg << std::endl;
	}};

	/* shaders */ {
		/* buffer */ {
			std::string vertex(std::get<0>(load_resource("/net/ldvsoft/spbau/gl/scene_vertex.glsl")));
			std::string fragment(std::get<0>(load_resource("/net/ldvsoft/spbau/gl/scene_fragment.glsl")));
			std::string error_string;
			gl.buffer_program = Program::build_program({{GL_VERTEX_SHADER, vertex}, {GL_FRAGMENT_SHADER, fragment}}, error_string);
			if (gl.buffer_program == nullptr) {
				report_error("Program Buffer: " + error_string);
				return;
			}
		}

		/* deferred */ {
			std::string vertex(std::get<0>(load_resource("/net/ldvsoft/spbau/gl/deferred_vertex.glsl")));
			std::string fragment(std::get<0>(load_resource("/net/ldvsoft/spbau/gl/deferred_fragment.glsl")));
			std::string error_string;
			gl.deferred_program = Program::build_program({{GL_VERTEX_SHADER, vertex}, {GL_FRAGMENT_SHADER, fragment}}, error_string);
			if (gl.deferred_program == nullptr) {
				report_error("Program Deferred: " + error_string);
				return;
			}
		}
	}

	/* framebuffer */ {
//		glGenFramebuffers(1, &gl.framebuffer);
//		glBindFramebuffer(GL_FRAMEBUFFER, gl.framebuffer);

//		glDrawBuffer(GL_NONE);

//		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
//			area->set_error(Error(hw3_error_quark, 0, "Failed to create framebuffer."));
//			glDeleteFramebuffers(1, &gl.framebuffer);
//			return;
//		}

//		glBindFramebuffer(GL_FRAMEBUFFER, 0);
//		glBindTexture(GL_TEXTURE_2D, 0);
	}

	/* scene */ {
		/* rabbit */ {
			::Object obj{::Object::load(std::get<0>(load_resource("/net/ldvsoft/spbau/gl/stanford_bunny.obj")))};
			obj.recalculate_normals();
			obj.normals_as_colors();
			gl.statue = std::make_unique<SceneObject>(obj);
			gl.statue->position = glm::translate(glm::vec3(0, -.03, 0));
		}

		/* light cube */ {
			::Object obj{::Object::load(std::get<0>(load_resource("/net/ldvsoft/spbau/gl/light_sphere.obj")))};
			gl.light_sphere = std::make_unique<SceneObject>(obj);
		}
	}

	std::cout << "Rendering on " << glGetString(GL_RENDERER) << std::endl;
}

void Hw2Window::gl_finit() {
	area->make_current();
	if (area->has_error())
		return;
	gl.statue = nullptr;
	gl.light_sphere = nullptr;
	gl.deferred_program = nullptr;
	gl.buffer_program = nullptr;
}

bool Hw2Window::gl_render(RefPtr<GLContext> const &context) {
	static_cast<void>(context);
//	check("Hw2Window::gl_render before...");

	if (area->has_error())
		return false;

	glClearColor(0, 0, 0, 1);

	/* animate */ {
		double a{cos(animation.progress * 10 * M_PI) * M_PI / 6}, b{animation.progress * 30 * M_PI}, r{.1};
		//std::cout << animation.progress << " " << a << " " << b << std::endl;
		gl.lights[0].position = glm::vec3(
			r * cos(a) * sin(b),
			.1 + r * sin(a),
			r * cos(a) * cos(b)
		);

		gl.statue->animation_position = glm::translate(glm::vec3(0, std::max<float>(0, sin(animation.progress * 4 * M_PI)) * .01, 0));
	}

//	/* shadowmap */ {
//		GLint old_buffer, old_vp[4];
//		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_buffer);
//		glGetIntegerv(GL_VIEWPORT, old_vp);
//
//		glBindFramebuffer(GL_FRAMEBUFFER, gl.framebuffer);
//		glViewport(0, 0, gl.shadowmap_size, gl.shadowmap_size);
//
//		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//		gl_render_shadowmap();
//
//		glBindFramebuffer(GL_FRAMEBUFFER, old_buffer);
//		glViewport(old_vp[0], old_vp[1], old_vp[2], old_vp[3]);
//	}

	glm::mat4 cam_proj{glm::perspective(
		/* vertical fov = */ glm::radians(gl.fov),
		/* ratio        = */ static_cast<float>(area->get_width()) / area->get_height(),
		/* planes       : */ .01f, 1000.0f
	)};

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	switch (display_mode_combobox->get_active_row_number()) {
	case SCENE:
		gl_render_buffer(get_camera_view(), cam_proj);
		break;
	case DEFERRED_LIGHTS:
		gl_render_lights(get_camera_view(), cam_proj);
		gl_render_buffer(get_camera_view(), cam_proj);
		break;
	}

	glFlush();

	return false;
}

void Hw2Window::gl_render_buffer(glm::mat4 const &view, glm::mat4 const &proj) {
	gl.buffer_program->use();

	glUniform3fv(gl.buffer_program->get_uniform("light_world"), 1, &gl.lights[0].position[0]);
	glUniform3fv(gl.buffer_program->get_uniform("light_color"), 1, &gl.lights[0].color[0]);
	glUniform1f (gl.buffer_program->get_uniform("light_power"), gl.lights[0].power);
	glUniform1f (gl.buffer_program->get_uniform("light_radius"), gl.lights[0].radius);

	gl_draw_objects(*gl.buffer_program, view, proj);

	glUseProgram(0);
}

void Hw2Window::gl_render_lights(glm::mat4 const &view, glm::mat4 const &proj) {
	gl.deferred_program->use();

	for ([[maybe_unused]] auto const &light: gl.lights) {
		gl.light_sphere->position =
			glm::translate(glm::mat4(), light.position) 
			* glm::scale(glm::vec3(light.radius, light.radius, light.radius));

		glUniform3fv(gl.deferred_program->get_uniform("light_world"), 1, &light.position[0]);
		glUniform3fv(gl.deferred_program->get_uniform("light_color"), 1, &light.color[0]);
		glUniform1f (gl.deferred_program->get_uniform("light_power"), light.power);
		glUniform1f (gl.deferred_program->get_uniform("light_radius"), light.radius);

		gl_draw_object(*gl.light_sphere, *gl.deferred_program, view, proj);
	}

	glUseProgram(0);
}

void Hw2Window::gl_draw_objects(Program const &program, glm::mat4 const &v, glm::mat4 const &p) {
	gl_draw_object(*gl.statue, program, v, p);
}

void Hw2Window::gl_draw_object(
	SceneObject const &object,
	Program const &program,
	glm::mat4 const &v,
	glm::mat4 const &p
) {
	auto pos{program.get_attribute("vertex_position_model")};
	auto nor{program.get_attribute("vertex_normal_model")};
	auto clr{program.get_attribute("vertex_color")};
	object.set_attribute_to_position(pos);
	object.set_attribute_to_normal(nor);
	object.set_attribute_to_color(clr);
	object.draw(
		v, p,
		program.get_uniform("m"),
		program.get_uniform("v"),
		program.get_uniform("p"),
		program.get_uniform("mv"),
		program.get_uniform("mvp")
	);
}

glm::mat4 Hw2Window::get_camera_view() const {
	return glm::rotate(navigation.yangle, glm::vec3(1, 0, 0)) *
		glm::rotate(navigation.xangle, glm::vec3(0, -1, 0)) *
		glm::translate(-navigation.camera_position);
}

void Hw2Window::animate_toggled() {
	bool state{animate->get_active()};
	if (state) {
		animation.state = animation.PENDING;
	} else {
		animation.state = animation.STOPPED;
	}
}

void Hw2Window::reset_position_clicked() {
	navigation.xangle = 0;
	navigation.yangle = 0;
	navigation.camera_position = glm::vec3(0, .2, .2);
	area->queue_render();
}

void Hw2Window::reset_animation_clicked() {
	animation.progress = 0;
	if (animation.state == animation.STARTED)
		animation.state = animation.PENDING;
	area->queue_render();
}

void Hw2Window::tick(gint64 new_time) {
	gint64 delta{new_time - animation.start_time};
	double seconds_delta{delta / static_cast<double>(G_USEC_PER_SEC)};
	/* animation */ {
		switch (animation.state) {
		case animation.PENDING:
			animation.start_time = new_time;
			animation.start_progress = animation.progress;
			animation.state = animation.STARTED;
			break;
		case animation.STARTED:
			animation.progress = fmod(animation.start_progress + seconds_delta * animation.progress_per_second, 1);
			area->queue_render();
			break;
		default:
			break;
		}
	}
	/* navigation */ {
		bool flag{false};
		glm::vec3 direction(0, 0, 0);
		if (navigation.to_left) {
			direction += glm::vec3(-1, 0, 0);
			flag = true;
		}
		if (navigation.to_right) {
			direction += glm::vec3(+1, 0, 0);
			flag = true;
		}
		if (navigation.to_up) {
			direction += glm::vec3(0, +1, 0);
			flag = true;
		}
		if (navigation.to_down) {
			direction += glm::vec3(0, -1, 0);
			flag = true;
		}
		if (navigation.to_front) {
			direction += glm::vec3(0, 0, -1);
			flag = true;
		}
		if (navigation.to_back) {
			direction += glm::vec3(0, 0, +1);
			flag = true;
		}
		if (flag) {
			direction *= view_range / 20;
			navigation.camera_position += glm::vec3(inverse(get_camera_view()) * glm::vec4(direction, 0));
			area->queue_render();
		}
	}
}

void Hw2Window::display_mode_changed() {
	area->queue_render();
}

bool Hw2Window::mouse_pressed(GdkEventButton *event) {
	navigation.pressed = true;
	navigation.start_xangle = navigation.xangle;
	navigation.start_yangle = navigation.yangle;
	navigation.start_x = event->x;
	navigation.start_y = event->y;
	return false;
}

bool Hw2Window::mouse_released(GdkEventButton *event) {
	static_cast<void>(event);
	navigation.pressed = false;
	return false;
}

bool Hw2Window::mouse_moved(GdkEventMotion *event) {
	if (!navigation.pressed)
		return false;

	//double vfov{gl.fov / 2}, hfov{atan(tan(gl.fov) * area->get_width() / area->get_height()) / 2};
	double rel_x{-1 + 2 * event->x / area->get_width()};
	double rel_y{+1 - 2 * event->y / area->get_height()};
	double rel_start_x{-1 + 2 * navigation.start_x / area->get_width()};
	double rel_start_y{+1 - 2 * navigation.start_y / area->get_height()};

	double dx{rel_x - rel_start_x};
	double dy{rel_y - rel_start_y};

	navigation.xangle = fmod(navigation.start_xangle + dx, 2 * M_PI);
	navigation.yangle = glm::clamp(navigation.start_yangle + dy, -M_PI / 2, +M_PI / 2);

	area->queue_render();
	return false;
}

bool Hw2Window::key_pressed(GdkEventKey *event) {
	switch (event->keyval) {
	case GDK_KEY_a:
		navigation.to_left = true;
		break;
	case GDK_KEY_d:
		navigation.to_right = true;
		break;
	case GDK_KEY_r:
		navigation.to_up = true;
		break;
	case GDK_KEY_f:
		navigation.to_down = true;
		break;
	case GDK_KEY_w:
		navigation.to_front = true;
		break;
	case GDK_KEY_s:
		navigation.to_back = true;
		break;
	}
	return false;
}

bool Hw2Window::key_released(GdkEventKey *event) {
	switch (event->keyval) {
	case GDK_KEY_a:
		navigation.to_left = false;
		break;
	case GDK_KEY_d:
		navigation.to_right = false;
		break;
	case GDK_KEY_r:
		navigation.to_up = false;
		break;
	case GDK_KEY_f:
		navigation.to_down = false;
		break;
	case GDK_KEY_w:
		navigation.to_front = false;
		break;
	case GDK_KEY_s:
		navigation.to_back = false;
		break;
	}
	return false;
}
