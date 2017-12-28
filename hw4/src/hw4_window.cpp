#include "hw4_window.hpp"
#include "hw4_error.hpp"

#include "marching_geometry_data.hpp"

#include <epoxy/gl.h>

#include <gdk/gdkkeysyms.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include <iostream>
#include <random>

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

unique_ptr<Hw4Window> Hw4Window::create() {
	auto builder{Gtk::Builder::create_from_resource("/net/ldvsoft/spbau/gl/hw4_window.ui")};
	Hw4Window *result;
	builder->get_widget_derived("Hw4Window", result);

	return unique_ptr<Hw4Window>(result);
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
	static_cast<Hw4Window *>(data)->tick(gdk_frame_clock_get_frame_time(clock));
	return G_SOURCE_CONTINUE;
}

Hw4Window::Hw4Window(
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
	spheres_adjustment = RefPtr<Gtk::Adjustment>::cast_dynamic(builder->get_object("spheres_adjustment"));

	area->set_has_depth_buffer();
	/* options */ {
		/* marching cubes */ {
			static_assert(MARCHING_CUBES == 0);
			auto &row{*display_mode_list_store->append()};
			row.set_value<Glib::ustring>(0, "Marching cubes");
		}

		display_mode_combobox->set_active(MARCHING_CUBES);
	}

	spheres_adjustment->set_value(1);

	area->signal_realize  ().connect(sigc::mem_fun(*this, &Hw4Window::gl_init));
	area->signal_unrealize().connect(sigc::mem_fun(*this, &Hw4Window::gl_finit), false);
	area->signal_render   ().connect(sigc::mem_fun(*this, &Hw4Window::gl_render));

	area_eventbox->signal_button_press_event  ().connect(sigc::mem_fun(*this, &Hw4Window::mouse_pressed));
	area_eventbox->signal_button_release_event().connect(sigc::mem_fun(*this, &Hw4Window::mouse_pressed));
	area_eventbox->signal_motion_notify_event ().connect(sigc::mem_fun(*this, &Hw4Window::mouse_moved));

	signal_key_press_event  ().connect(sigc::mem_fun(*this, &Hw4Window::key_pressed));
	signal_key_release_event().connect(sigc::mem_fun(*this, &Hw4Window::key_released));

	animate->signal_toggled().connect(sigc::mem_fun(*this, &Hw4Window::animate_toggled));
	reset_position->signal_clicked ().connect(sigc::mem_fun(*this, &Hw4Window::reset_position_clicked));
	reset_animation->signal_clicked().connect(sigc::mem_fun(*this, &Hw4Window::reset_animation_clicked));
	spheres_adjustment->signal_value_changed().connect(sigc::mem_fun(*this, &Hw4Window::spheres_changed));
	display_mode_combobox->signal_changed().connect(sigc::mem_fun(*this, &Hw4Window::display_mode_changed));

	view_range = .2;

	spheres_changed();
	reset_position_clicked();
	reset_animation_clicked();
}

Hw4Window::~Hw4Window() {
	gtk_widget_remove_tick_callback(GTK_WIDGET(area->gobj()), ticker_id);
}

void Hw4Window::gl_init() {
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
		Error error(hw4_error_quark, 0, msg);
		area->set_error(error);
		std::cout << "ERROR: " << msg << std::endl;
	}};

	/* shaders */ {
		std::string error_string;
		std::string vertex  (std::get<0>(load_resource("/net/ldvsoft/spbau/gl/marching_vertex.glsl")));
		std::string geometry(std::get<0>(load_resource("/net/ldvsoft/spbau/gl/marching_geometry.glsl")));
		std::string fragment(std::get<0>(load_resource("/net/ldvsoft/spbau/gl/marching_fragment.glsl")));
		gl.marching_program = Program::build_program(
			{{GL_VERTEX_SHADER, vertex}, {GL_GEOMETRY_SHADER, geometry}, {GL_FRAGMENT_SHADER, fragment}},
			error_string
		);
		if (gl.marching_program == nullptr) {
			report_error("Marching cubes program: " + error_string);
			return;
		}

	}

	/* framebuffer */ {
		glGenFramebuffers(1, &gl.framebuffer);
	}

	/* marching: data */ {
		/* base positions */ {
			glGenTextures(1, &gl.geometry_base_positions);
			glBindTexture(GL_TEXTURE_1D, gl.geometry_base_positions);
			glTexImage1D(GL_TEXTURE_1D, 0,
				GL_RGB16F, gl_data::base_positions_count,
				0, GL_RGB, GL_FLOAT, gl_data::base_positions
			);
			glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_1D, 0);
		}
		/* edges */ {
			glGenTextures(1, &gl.geometry_edges);
			glBindTexture(GL_TEXTURE_1D, gl.geometry_edges);
			glTexImage1D(GL_TEXTURE_1D, 0,
				GL_R8, gl_data::edges_count,
				0, GL_RED, GL_BYTE, gl_data::edges
			);
			glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_1D, 0);
		}
		/* case_sizes */ {
			glGenTextures(1, &gl.geometry_case_sizes);
			glBindTexture(GL_TEXTURE_1D, gl.geometry_case_sizes);
			glTexImage1D(GL_TEXTURE_1D, 0,
				GL_R8, gl_data::cases_count,
				0, GL_RED, GL_BYTE, gl_data::case_sizes
			);
			glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_1D, 0);
		}
	}

	std::cout << "Rendering on " << glGetString(GL_RENDERER) << std::endl;
}

void Hw4Window::gl_finit() {
	area->make_current();
	if (area->has_error())
		return;
	glDeleteTextures(1, &gl.geometry_case_sizes);
	glDeleteTextures(1, &gl.geometry_edges);
	glDeleteTextures(1, &gl.geometry_base_positions);
	glDeleteFramebuffers(1, &gl.framebuffer);
	gl.marching_program = nullptr;
}

bool Hw4Window::gl_render(RefPtr<GLContext> const &context) {
	static_cast<void>(context);

	if (area->has_error())
		return false;

	GLsizei height{area->get_height()}, width{area->get_width()};
	GLint old_buffer, old_vp[4];
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_buffer);
	glGetIntegerv(GL_VIEWPORT, old_vp);

	/* animate */ {
		for (auto &sphere: gl.spheres) {
			double angle(fmod(animation.progress * sphere.speed, 2 * M_PI));
			double a{cos(angle) * M_PI / 6};
			double b{angle * 3 * sphere.speed};
			sphere.position = glm::vec3(
				sphere.radius * cos(a) * sin(b),
				sphere.radius * sin(a) + .15,
				sphere.radius * cos(a) * cos(b)
			);
		}
	}

	glm::mat4 cam_proj{glm::perspective(
		/* vertical fov = */ glm::radians(gl.fov),
		/* ratio        = */ static_cast<float>(area->get_width()) / area->get_height(),
		/* planes       : */ .005f, 100.0f
	)};

	glClearColor(0, 0, 0, 1);

	/* buffer */ if(false) {
		glBindFramebuffer(GL_FRAMEBUFFER, gl.framebuffer);
		glViewport(0, 0, width, height);

		/* buffers */ {
			constexpr size_t cnt{0};
			GLenum buffers[cnt]{};
			glDrawBuffers(cnt, buffers);
		}
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			area->set_error(Error(hw4_error_quark, 0, "Failed to create framebuffer."));
			return false;
		}

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glBindFramebuffer(GL_FRAMEBUFFER, old_buffer);
		glViewport(old_vp[0], old_vp[1], old_vp[2], old_vp[3]);
	}

	static constexpr bool useDebug{false};
	GLuint tmp, out;
	if (useDebug) {
		glGenTextures(1, &out);
		glBindTexture(GL_TEXTURE_2D, out);
		glTexImage2D(
			GL_TEXTURE_2D, 0,
			GL_RGB16F, area->get_width(), area->get_height(),
			0, GL_RGB, GL_FLOAT, nullptr
		);
		glBindTexture(GL_TEXTURE_2D, 0);

		glGenFramebuffers(1, &tmp);
		glBindFramebuffer(GL_FRAMEBUFFER, tmp);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, out, 0);
		{
			constexpr size_t cnt{1};
			constexpr GLenum buffers[cnt]{GL_COLOR_ATTACHMENT0};
			glDrawBuffers(cnt, buffers);
		}
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			area->set_error(Error(hw4_error_quark, 0, "TMP"));
			return false;
		}
	}

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	switch (display_mode_combobox->get_active_row_number()) {
	case MARCHING_CUBES:
		gl_render_marching(get_camera_view(), cam_proj);
		break;
	}

	if (useDebug) {
		glBindFramebuffer(GL_FRAMEBUFFER, old_buffer);
		glDeleteTextures(1, &out);
		glDeleteFramebuffers(1, &tmp);
	}

	glFlush();

	return false;
}

void Hw4Window::gl_render_marching(glm::mat4 const &view, glm::mat4 const &proj) {
	gl.marching_program->use();

	size_t resolution{40};
	float delta = view_range / resolution;

	size_t points_count{resolution * resolution * resolution};
	std::unique_ptr<glm::vec3[]> points(new glm::vec3[points_count]);
	for (size_t i{0}; i != resolution; ++i)
		for (size_t j{0}; j != resolution; ++j)
			for (size_t k{0}; k != resolution; ++k)
				points[(i * resolution + j) * resolution + k] = glm::vec3(
					(2.0 * i + 1) / resolution - 1,
					(2.0 * j + 1) / resolution - 1,
					(2.0 * k + 1) / resolution - 1
				) * view_range;

	GLuint vao, buffer;
	/* create vao */ {
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		glGenBuffers(1, &buffer);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * points_count, points.get(), GL_STATIC_DRAW);
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_1D, gl.geometry_base_positions);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_1D, gl.geometry_edges);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_1D, gl.geometry_case_sizes);

	glUniform1i(gl.marching_program->get_uniform("geometry_base_positions"), 0);
	glUniform1i(gl.marching_program->get_uniform("geometry_edges"), 1);
	glUniform1i(gl.marching_program->get_uniform("geometry_case_sizes"), 2);

	glUniform1f(gl.marching_program->get_uniform("dx"), delta);
	glUniform1f(gl.marching_program->get_uniform("dy"), delta);
	glUniform1f(gl.marching_program->get_uniform("dz"), delta);

	glm::mat4 vp(proj * view);
	glUniformMatrix4fv(gl.marching_program->get_uniform("vp"), 1, GL_FALSE, &vp[0][0]);

	GLuint pos{gl.marching_program->get_attribute("vertex_position_world")};
	glEnableVertexAttribArray(pos);
	glVertexAttribPointer(pos,
		3, GL_FLOAT,
		GL_FALSE,
		sizeof(glm::vec3), nullptr
	);

	glDrawArrays(GL_POINTS, 0, points_count);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_1D, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_1D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_1D, 0);

	/* delete vao */ {
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glDeleteBuffers(1, &buffer);
		glBindVertexArray(0);
		glDeleteVertexArrays(1, &vao);
	}

	glUseProgram(0);
}

glm::mat4 Hw4Window::get_camera_view() const {
	return glm::rotate(navigation.yangle, glm::vec3(1, 0, 0)) *
		glm::rotate(navigation.xangle, glm::vec3(0, -1, 0)) *
		glm::translate(-navigation.camera_position);
}

void Hw4Window::animate_toggled() {
	bool state{animate->get_active()};
	if (state) {
		animation.state = animation.PENDING;
	} else {
		animation.state = animation.STOPPED;
	}
}

void Hw4Window::reset_position_clicked() {
	navigation.xangle = 0;
	navigation.yangle = 0;
	navigation.camera_position = glm::vec3(0, .2, .2);
	area->queue_render();
}

void Hw4Window::reset_animation_clicked() {
	animation.progress = 0;
	if (animation.state == animation.STARTED)
		animation.state = animation.PENDING;
	area->queue_render();
}

void Hw4Window::tick(gint64 new_time) {
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
			animation.progress = animation.start_progress + seconds_delta * animation.progress_per_second;
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

void Hw4Window::spheres_changed() {
	size_t new_size(spheres_adjustment->get_value()), old_size{gl.spheres.size()};
	gl.spheres.resize(new_size);
	for (size_t i{old_size}; i < new_size; ++i) {
		std::random_device rd;
		std::default_random_engine rand(rd());
		std::uniform_real_distribution<double> dist;
		double const r{.20};
		gl.spheres[i].power = dist(rand) * .02;
		gl.spheres[i].speed = (dist(rand) * 4 + 8) * M_PI / 5;
		gl.spheres[i].radius = r;
	}
}

void Hw4Window::display_mode_changed() {
	area->queue_render();
}

bool Hw4Window::mouse_pressed(GdkEventButton *event) {
	navigation.pressed = true;
	navigation.start_xangle = navigation.xangle;
	navigation.start_yangle = navigation.yangle;
	navigation.start_x = event->x;
	navigation.start_y = event->y;
	return false;
}

bool Hw4Window::mouse_released(GdkEventButton *event) {
	static_cast<void>(event);
	navigation.pressed = false;
	return false;
}

bool Hw4Window::mouse_moved(GdkEventMotion *event) {
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

bool Hw4Window::key_pressed(GdkEventKey *event) {
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

bool Hw4Window::key_released(GdkEventKey *event) {
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
