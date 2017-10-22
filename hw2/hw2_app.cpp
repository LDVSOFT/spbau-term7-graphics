#include "hw2_app.hpp"

using Glib::RefPtr;
using Gtk::Application;

RefPtr<Hw2App> Hw2App::create(int argc, char *argv[]) {
	return RefPtr<Hw2App>(new Hw2App(argc, argv));
}

Hw2App::Hw2App(int argc, char *argv[]):
	Application(argc, argv, "net.ldvsoft.spbau.gl"),
	window(nullptr) {}

Hw2App::~Hw2App() = default;

void Hw2App::on_activate() {
	Application::on_activate();
	
	window = Hw2Window::create();
	window->show();
	add_window(*window);
}
