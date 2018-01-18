#pragma once

#define GLM_FORCE_SWIZZLE

#define CL_HPP_TARGET_OPENCL_VERSION  120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_ENABLE_EXCEPTIONS

#include "scene_object.hpp"
#include "program.hpp"

#include <glm/glm.hpp>

#include <gtkmm/builder.h>
#include <gtkmm/button.h>
#include <gtkmm/combobox.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/glarea.h>
#include <gtkmm/liststore.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/window.h>

#include <epoxy/gl.h>
#include <CL/cl2.hpp>

#include <memory>

class Hw4Window: public Gtk::Window {
private:
	Glib::RefPtr<Gtk::Builder> builder;

	Gtk::GLArea *area;
	Gtk::EventBox *area_eventbox;
	Gtk::ToggleButton *animate;
	Gtk::ComboBox *display_mode_combobox;
	Gtk::Button *reset_position, *reset_animation;

	Glib::RefPtr<Gtk::ListStore> display_mode_list_store;
	Glib::RefPtr<Gtk::Adjustment>
		spheres_adjustment,
		threshold_adjustment,
		xresolution_adjustment,
		yresolution_adjustment,
		zresolution_adjustment;

	enum display_mode_t {
		MARCHING_CUBES,
		SPHERES
	};

	struct _gl {
		std::unique_ptr<Program>
			marching_program,
			spheres_program;

		static float constexpr fov{60};
		GLuint framebuffer;

		struct sphere {
			glm::vec3 position;
			float power;
			float speed;
			float radius;
		};

		std::vector<sphere> spheres;
		std::unique_ptr<SceneObject> sphere, cube, mesh;
	} gl;

	struct _cl {
		cl::Device device;
		cl::Context context;
		cl::CommandQueue queue;
		cl::Kernel
			fill_values,
			find_edges,
			put_vertices,
			build_mesh;
	} cl;

	guint ticker_id;
	float view_range;

	struct {
		enum {
			STOPPED,
			PENDING,
			STARTED
		} state{STOPPED};
		gint64 start_time;
		float progress{0};
		double const progress_per_second{.08};
		float start_progress;
	} animation;

	struct {
		float xangle{0}, yangle{0};
		glm::vec3 camera_position;

		bool
			to_left{false}, to_right{false},
			to_up{false}, to_down{false},
			to_front{false}, to_back{false};

		bool pressed{false};
		float start_xangle, start_yangle;
		double start_x, start_y;
	} navigation;

	void gl_init();
	void gl_finit();
	bool gl_render(Glib::RefPtr<Gdk::GLContext> const &context);
	void gl_render_marching(glm::mat4 const &view, glm::mat4 const &proj);
	void gl_render_spheres(glm::mat4 const &view, glm::mat4 const &proj);
	void gl_draw_object(SceneObject const &object, Program const &program, glm::mat4 const &v, glm::mat4 const &p);

	glm::mat4 get_camera_view() const;

	void animate_toggled();
	void reset_position_clicked();
	void reset_animation_clicked();
	void spheres_changed();
	void options_changed();
	bool mouse_pressed(GdkEventButton *event);
	bool mouse_released(GdkEventButton *event);
	bool mouse_moved(GdkEventMotion *event);
	bool key_pressed(GdkEventKey *event);
	bool key_released(GdkEventKey *event);

public:
	static std::unique_ptr<Hw4Window> create();

	Hw4Window(BaseObjectType *type, Glib::RefPtr<Gtk::Builder> const &builder);
	~Hw4Window() override;

	void tick(gint64 time);
};
