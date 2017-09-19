#ifndef __HW1_ERROR_H__
#define __HW1_ERROR_H__

#include <glib.h>

G_BEGIN_DECLS

#define HW1_ERROR (hw1_error_quark())

typedef enum {
	HW1_ERROR_SHADER_COMPILATION,
	HW1_ERROR_SHADER_LINK
} hw1Error;

GQuark hw1_error_quark(void);

G_END_DECLS

#endif /* __HW1_ERROR_H__ */
