#include "hw2_window.hpp"

#include <iostream>

using Gdk::GLContext;
using Glib::RefPtr;
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

Hw2Window::Hw2Window(
	BaseObjectType *type,
	RefPtr<Builder> const &builder
):
	Window(type),
	builder(builder)
{
	builder->get_widget("draw_area", area);
	
	area->signal_realize().connect(sigc::mem_fun(*this, &Hw2Window::gl_init));
	area->signal_unrealize().connect(sigc::mem_fun(*this, &Hw2Window::gl_finit));
	area->signal_render().connect(sigc::mem_fun(*this, &Hw2Window::gl_render));
}

Hw2Window::~Hw2Window() = default;

void Hw2Window::gl_init() {
	std::cout << "GL init" << std::endl;
}

void Hw2Window::gl_finit() {
	std::cout << "GL finit" << std::endl;
}

bool Hw2Window::gl_render(RefPtr<GLContext> const &context) {
	(void) context;

	std::cout << "GL render" << std::endl;
	return false;
}
