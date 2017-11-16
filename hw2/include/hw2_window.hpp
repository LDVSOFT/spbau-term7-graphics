#pragma once

#include "scene_object.hpp"
#include "program.hpp"

#include <glm/mat4x4.hpp>

#include <gtkmm/builder.h>
#include <gtkmm/button.h>
#include <gtkmm/combobox.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/glarea.h>
#include <gtkmm/liststore.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/window.h>

#include <epoxy/gl.h>

#include <memory>

class Hw2Window: public Gtk::Window {
private:
	Glib::RefPtr<Gtk::Builder> builder;

	Gtk::GLArea *area;
	Gtk::EventBox *area_eventbox;
	Gtk::ToggleButton *animate;
	Gtk::ComboBox *display_mode_combobox;
	Gtk::Button *reset_position, *reset_animation;

	Glib::RefPtr<Gtk::ListStore> display_mode_list_store;

	enum display_mode_t {
		SCENE,
		SCENE_FROM_SUN,
		SHADOWMAP
	};

	struct _gl {
		std::unique_ptr<Program> scene_program, shadowmap_program;

		static GLsizei constexpr shadowmap_size{2048};
		static float constexpr pov{60};
		GLuint framebuffer;
		GLuint shadowmap;

		glm::vec3 light_position;
		glm::vec3 light_color;
		float light_power;

		glm::vec3 sun_position;
		glm::vec3 sun_color;
		float sun_power;
		float sun_view_range;

		glm::mat4 sun_proj, sun_view;

		float angle{0};
		std::unique_ptr<SceneObject> object, base_plane;
	} gl;

	struct {
		enum {
			STOPPED,
			PENDING,
			STARTED
		} state{STOPPED};
		guint id;
		gint64 start_time;
		double const angle_per_second{M_PI / 2};
		float start_angle;
	} animation;

	struct {
		float xangle{0}, yangle{0};

		bool pressed{false};
		float start_xangle, start_yangle;
		double start_x, start_y;
	} navigation;

	void gl_init();
	void gl_finit();
	bool gl_render(Glib::RefPtr<Gdk::GLContext> const &context);
	void gl_render_scene(glm::mat4 const &view, glm::mat4 const &proj);
	void gl_render_shadowmap();
	void gl_draw_objects(Program const &program, glm::mat4 const &v, glm::mat4 const &p);

	void animate_toggled();
	void reset_position_clicked();
	void reset_animation_clicked();
	void display_mode_changed();
	bool mouse_pressed(GdkEventButton *event);
	bool mouse_released(GdkEventButton *event);
	bool mouse_moved(GdkEventMotion *event);

public:
	static std::unique_ptr<Hw2Window> create();

	Hw2Window(BaseObjectType *type, Glib::RefPtr<Gtk::Builder> const &builder);
	~Hw2Window() override;

	void animate_tick(gint64 time);
};
