#pragma once

#include <glm/mat4x4.hpp>

#include <gtkmm/builder.h>
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

	struct {
		struct {
			GLuint program;
			GLuint mvp_location;
			GLuint position_location;
		} shader;

		struct {
			glm::mat4 projection;
			glm::mat4 mvp;

			GLuint vao;
		} scene;
	} gl;

	void gl_init();
	void gl_finit();
	bool gl_render(Glib::RefPtr<Gdk::GLContext> const &context);

	void animate_toggled();

public:
	static std::unique_ptr<Hw2Window> create();

	Hw2Window(BaseObjectType *type, Glib::RefPtr<Gtk::Builder> const &builder);
	~Hw2Window() override;
};
