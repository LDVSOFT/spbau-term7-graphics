#include <gtkmm/builder.h>
#include <gtkmm/glarea.h>
#include <gtkmm/window.h>

#include <memory>

class Hw2Window: public Gtk::Window {
private:
	Glib::RefPtr<Gtk::Builder> builder;
	Gtk::GLArea *area;

	void gl_init();
	void gl_finit();
	bool gl_render(Glib::RefPtr<Gdk::GLContext> const &context);

public:
	static std::unique_ptr<Hw2Window> create();

	Hw2Window(BaseObjectType *type, Glib::RefPtr<Gtk::Builder> const &builder);
	~Hw2Window() override;
};
