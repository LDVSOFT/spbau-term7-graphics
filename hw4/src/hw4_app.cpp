#include "hw4_app.hpp"

using Gio::Resource;
using Glib::RefPtr;
using Gtk::Application;

RefPtr<Hw4App> Hw4App::create(int argc, char *argv[]) {
	return RefPtr<Hw4App>(new Hw4App(argc, argv));
}

Hw4App::Hw4App(int argc, char *argv[]):
	Application(argc, argv, "net.ldvsoft.spbau.gl") {
	Resource::create_from_file("hw4.gresource")->register_global();
}

Hw4App::~Hw4App() = default;

void Hw4App::on_activate() {
	Application::on_activate();
	
	auto window{Hw4Window::create()};
	window->show();
	add_window(*window);

	// FIXME HACK here, need to find out a better way...
	window.release();
}
