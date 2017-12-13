#pragma once

#include "hw3_window.hpp"

#include <gtkmm/application.h>

#include <memory>

class Hw2App: public Gtk::Application {
private:
	Hw2App(int argc, char *argv[]);
	virtual ~Hw2App() override;

	virtual void on_activate() override;
public:
	static Glib::RefPtr<Hw2App> create(int argc, char *argv[]);
};
