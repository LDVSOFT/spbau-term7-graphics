#include "hw3_app.hpp"

using Glib::RefPtr;
using Gtk::Application;

RefPtr<Hw3App> Hw3App::create(int argc, char *argv[]) {
	return RefPtr<Hw3App>(new Hw3App(argc, argv));
}

Hw3App::Hw3App(int argc, char *argv[]):
	Application(argc, argv, "net.ldvsoft.spbau.gl") {}

Hw3App::~Hw3App() = default;

void Hw3App::on_activate() {
	Application::on_activate();
	
	auto window{Hw3Window::create()};
	window->show();
	add_window(*window);

	// FIXME HACK here, need to find out a better way...
	window.release();
}
