#include "hw2_window.hpp"
#include "hw2_error.hpp"

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

	gl.light_position = glm::vec3(0, .1, .5);
	gl.light_color = glm::vec3(1, 1, 1);
	gl.light_power = .04;

	gl.sun_position = glm::vec3(-.1, .1, -.1);
	gl.sun_color = glm::vec3(1, .95, .5);
	gl.sun_power = .9;
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
			for (int i{0}; i != gl.acolytes_count; ++i) {
				auto a{static_cast<float>(2 * M_PI / gl.acolytes_count * i)};
				gl.acolytes[i] = std::make_unique<SceneObject>(obj);
				gl.acolytes[i]->position = glm::rotate(a, glm::vec3(0, 1, 0))
					* glm::translate(glm::vec3(.18, 0, 0))
					* glm::scale(glm::vec3(.3, .3, .3))
					* glm::translate(glm::vec3(0, -.03, 0));
			}

			obj.normals_as_colors();
			gl.statue = std::make_unique<SceneObject>(obj);
			gl.statue->position = glm::translate(glm::vec3(0, -.03, 0));
		}

		/* base plane */ {
			::Object obj{::Object::load(std::get<0>(load_resource("/net/ldvsoft/spbau/gl/plane.obj")))};
			gl.base_plane = std::make_unique<SceneObject>(obj);
		}
		gl.sun_proj = glm::ortho<float>(
			/* X ragne : */ -view_range, +view_range,
			/* Y ragne : */ -view_range, +view_range,
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
	for (int i{0}; i != gl.acolytes_count; ++i)
		gl.acolytes[i] = nullptr;
	gl.statue = nullptr;
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

	/* animate */ {
		double a{cos(animation.progress * 10 * M_PI) * M_PI / 6}, b{animation.progress * 30 * M_PI}, r{.3};
		//std::cout << animation.progress << " " << a << " " << b << std::endl;
		gl.light_position = glm::vec3(
			r * cos(a) * sin(b),
			.2 + r * sin(a),
			r * cos(a) * cos(b)
		);

		gl.statue->animation_position = glm::translate(glm::vec3(0, std::max<float>(0, sin(animation.progress * 4 * M_PI)) * .01, 0));
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

	glm::mat4 cam_proj{glm::perspective(
		/* vertical fov = */ glm::radians(gl.fov),
		/* ratio        = */ static_cast<float>(area->get_width()) / area->get_height(),
		/* planes       : */ .01f, 1000.0f
	)};

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	switch (display_mode_combobox->get_active_row_number()) {
	case SCENE:
		gl_render_scene(get_camera_view(), cam_proj);
		break;
	case SCENE_FROM_SUN:
		gl_render_scene(gl.sun_view, gl.sun_proj);
		break;
	case SHADOWMAP:
		gl_render_shadowmap();
		break;
	}

	glFlush();

	return false;
}

void Hw2Window::gl_render_scene(glm::mat4 const &view, glm::mat4 const &proj) {
	gl.scene_program->use();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gl.shadowmap);

	glm::mat4 shadowmap_vp{gl.sun_proj * gl.sun_view};

	glUniform3fv(gl.scene_program->get_uniform("light_world"), 1, &gl.light_position[0]);
	glUniform3fv(gl.scene_program->get_uniform("light_color"), 1, &gl.light_color[0]);
	glUniform1f (gl.scene_program->get_uniform("light_power"), gl.light_power);

	glUniform3fv(gl.scene_program->get_uniform("tosun_world"), 1, &gl.sun_position[0]);
	glUniform3fv(gl.scene_program->get_uniform("sun_color"  ), 1, &gl.sun_color[0]);
	glUniform1f (gl.scene_program->get_uniform("sun_power"), gl.sun_power);

	glUniform1i (gl.scene_program->get_uniform("shadowmap"), 0 /*gl.shadowmap*/);
	glUniformMatrix4fv(
		gl.scene_program->get_uniform("shadowmap_vp"),
		1, GL_FALSE,
		&shadowmap_vp[0][0]
	);

	gl_draw_objects(*gl.scene_program, view, proj);

	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
}

void Hw2Window::gl_render_shadowmap() {
	gl.shadowmap_program->use();

	gl_draw_objects(*gl.shadowmap_program, gl.sun_view, gl.sun_proj);

	glUseProgram(0);
}

void Hw2Window::gl_draw_objects(Program const &program, glm::mat4 const &v, glm::mat4 const &p) {
	auto draw_object{[&](SceneObject const *object) -> void {
		auto pos{program.get_attribute("vertex_position_model")};
		auto nor{program.get_attribute("vertex_normal_model")};
		auto clr{program.get_attribute("vertex_color")};
		if (pos != Program::no_id)
			object->set_attribute_to_position(pos);
		if (nor != Program::no_id)
			object->set_attribute_to_normal(nor);
		if (clr != Program::no_id)
			object->set_attribute_to_color(clr);
		object->draw(
			v, p,
			program.get_uniform("m"), program.get_uniform("v"), program.get_uniform("p"),
			program.get_uniform("mv"), program.get_uniform("mvp")
		);
	}};

	draw_object(gl.base_plane.get());
	draw_object(gl.statue.get());
	for (int i{0}; i != gl.acolytes_count; ++i)
		draw_object(gl.acolytes[i].get());
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
