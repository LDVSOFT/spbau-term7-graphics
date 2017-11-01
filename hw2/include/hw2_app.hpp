#pragma once

#include "hw2_window.hpp"

#include <gtkmm/application.h>

#include <memory>

class Hw2App: public Gtk::Application {
private:
	std::unique_ptr<Hw2Window> window;

	Hw2App(int argc, char *argv[]);
	virtual ~Hw2App() override;

	virtual void on_activate() override;
public:
	static Glib::RefPtr<Hw2App> create(int argc, char *argv[]);
};
