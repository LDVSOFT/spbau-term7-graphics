#pragma once

#include "hw3_window.hpp"

#include <gtkmm/application.h>

#include <memory>

class Hw3App: public Gtk::Application {
private:
	Hw3App(int argc, char *argv[]);
	virtual ~Hw3App() override;

	virtual void on_activate() override;
public:
	static Glib::RefPtr<Hw3App> create(int argc, char *argv[]);
};
