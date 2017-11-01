#include "hw2_app.hpp"
#include "object.hpp"

#include <iostream>
#include <iterator>
#include <fstream>

int main(int argc, char **argv) {
//	(void) argc;
//	(void) argv;
//	/* small test */ {
//		std::ifstream f("stanford_bunny.obj");
//		std::string str(std::istreambuf_iterator<char>{f}, {});
//		auto obj{object::load(str)};
//		obj.recalculate_normals();
//		std::cout << obj << std::endl;
//	}

	try {
		auto app{Hw2App::create(argc, argv)};
		app->run();
	} catch (Glib::Error &e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}

	return 0;
}
