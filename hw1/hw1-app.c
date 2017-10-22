#include "hw1-app.h"
#include "hw1-app-window.h"

struct _Hw1App {
	GtkApplication parent_instance;
	GtkWidget *window;
};

struct _Hw1AppClass {
	GtkApplicationClass parent_class;
};

G_DEFINE_TYPE(Hw1App, hw1_app, GTK_TYPE_APPLICATION)

static void quit_activated(
		GSimpleAction *action,
		GVariant *parameter,
		gpointer app
) {
	(void) action;
	(void) parameter;
	g_application_quit(G_APPLICATION(app));
}

static GActionEntry app_entries[] = {
	{ "quit", quit_activated, NULL, NULL, NULL, {0} }
};

static void hw1_app_startup(GApplication *app) {
	GtkBuilder *builder;
	GMenuModel *app_menu;

	G_APPLICATION_CLASS(hw1_app_parent_class)->startup(app);

	g_action_map_add_action_entries(G_ACTION_MAP(app),
			app_entries, G_N_ELEMENTS(app_entries),
			app
	);

	builder = gtk_builder_new_from_resource("/net/ldvsoft/spbau/gl/hw1-app-menu.ui");
	app_menu = G_MENU_MODEL(gtk_builder_get_object(builder, "appmenu"));
	gtk_application_set_app_menu(GTK_APPLICATION(app), app_menu);
	g_object_unref(builder);
}

static void hw1_app_activate(GApplication *app) {
	Hw1App *self = HW1_APP(app);

	if (self->window == NULL)
		self->window = hw1_app_window_new(self);

	gtk_window_present(GTK_WINDOW(self->window));
}


static void hw1_app_class_init(Hw1AppClass *klass) {
	GApplicationClass *app_class = G_APPLICATION_CLASS(klass);

	app_class->startup = hw1_app_startup;
	app_class->activate = hw1_app_activate;
}

static void hw1_app_init(Hw1App *self) {
	(void) self;
}

GtkApplication *hw1_app_new(void) {
	return g_object_new(hw1_app_get_type(), "application-id", "io.bassi.hw1", NULL);
}
