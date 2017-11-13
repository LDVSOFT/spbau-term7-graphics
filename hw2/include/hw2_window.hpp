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
	Gtk::Button *reset;

	Glib::RefPtr<Gtk::ListStore> display_mode_list_store;

	enum display_mode_t {
		SCENE,
		SCENE_FROM_SUN,
		SHADOWMAP
	};

	struct _gl {
		std::unique_ptr<Program> scene_program, shadowmap_program;

		static GLsizei constexpr depth_buffer_size{2048};
		GLuint framebuffer;

		glm::vec3 light_position;
		glm::vec3 light_color;
		float light_power;

		glm::vec3 sun_position;
		glm::vec3 sun_color;
		float sun_power;
		float sun_view_range;

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

	void gl_init();
	void gl_finit();
	bool gl_render(Glib::RefPtr<Gdk::GLContext> const &context);
	void gl_render_scene();
	void gl_render_scene_from_sun();
	void gl_render_shadowmap();
	void gl_draw_objects(Program const &program, glm::mat4 const &v, glm::mat4 const &p);

	void animate_toggled();
	void reset_clicked();
	void display_mode_changed();

public:
	static std::unique_ptr<Hw2Window> create();

	Hw2Window(BaseObjectType *type, Glib::RefPtr<Gtk::Builder> const &builder);
	~Hw2Window() override;

	void animate_tick(gint64 time);
};
