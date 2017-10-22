#ifndef __HW1_APP_WINDOW_H__
#define __HW1_APP_WINDOW_H__

#include <gtk/gtk.h>
#include "hw1-app.h"

G_BEGIN_DECLS

#define hw1_TYPE_APP_WINDOW (hw1_app_window_get_type())

G_DECLARE_FINAL_TYPE(Hw1AppWindow, hw1_app_window, HW1, APP_WINDOW, GtkApplicationWindow)

GtkWidget *hw1_app_window_new(Hw1App *app);

G_END_DECLS

#endif /* __HW1_APP_WINDOW_H__ */
