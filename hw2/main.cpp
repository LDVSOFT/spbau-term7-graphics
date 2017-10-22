#include "hw2_app.hpp"

#include <iostream>

int main(int argc, char **argv) {
	try {
		auto app{Hw2App::create(argc, argv)};
		app->run();
	} catch (Glib::Error &e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	return 0;
}
