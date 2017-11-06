#pragma once

#include "scene_object.hpp"
#include "program.hpp"

#include <glm/mat4x4.hpp>

#include <gtkmm/builder.h>
#include <gtkmm/button.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/glarea.h>
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
	Gtk::Button *reset;

	struct {
		struct {
			GLuint m{0};
			GLuint v{0};
			GLuint mv{0};
			GLuint mvp{0};

			GLuint light_position{0};
			GLuint light_power{0};
			GLuint light_color{0};

			GLuint vertex_position{0};
			GLuint vertex_normal{0};
		} locations;
		std::unique_ptr<Program> program;

		glm::mat4 camera;
		glm::mat4 perspective;

		float angle{0};
		std::unique_ptr<SceneObject> object;
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

	void animate_toggled();
	void update_camera();

public:
	static std::unique_ptr<Hw2Window> create();

	Hw2Window(BaseObjectType *type, Glib::RefPtr<Gtk::Builder> const &builder);
	~Hw2Window() override;

	void animate_tick(gint64 time);
};
