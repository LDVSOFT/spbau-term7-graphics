#ifndef __HW1_APP_H__
#define __HW1_APP_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define HW1_TYPE_APP (hw1_app_get_type())

G_DECLARE_FINAL_TYPE(Hw1App, hw1_app, HW1, APP, GtkApplication)

GtkApplication *hw1_app_new(void);

G_END_DECLS

#endif /* __HW1_APP_H__ */
