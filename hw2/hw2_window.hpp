#pragma once

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
			GLuint program{0};
			GLuint mvp_location{0};
			GLuint position_location{0};
			GLuint color_location{0};
		} shader;

		struct {
			glm::mat4 camera;
			glm::mat4 projection;
			glm::mat4 mvp;

			GLuint vao{0};
			GLuint elements_buffer{0};

			float angle{0};
		} scene;
	} gl;

	enum {
		STOPPED,
		PENDING,
		STARTED
	} animation_state{STOPPED};
	guint animation_id;
	gint64 animation_start_time;
	double const anglePerSecond{M_PI / 2};
	float animation_start_angle;

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
