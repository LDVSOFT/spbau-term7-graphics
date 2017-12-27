#pragma once

#include "hw4_window.hpp"

#include <gtkmm/application.h>

#include <memory>

class Hw4App: public Gtk::Application {
private:
	Hw4App(int argc, char *argv[]);
	virtual ~Hw4App() override;

	virtual void on_activate() override;
public:
	static Glib::RefPtr<Hw4App> create(int argc, char *argv[]);
};
