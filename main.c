#include <gtk/gtk.h>

#include "hw1-app.h"

int main(int argc, char *argv[]) {
	return g_application_run(G_APPLICATION(hw1_app_new()), argc, argv);
}
