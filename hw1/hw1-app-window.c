#include "hw1-app-window.h"
#include "hw1-error.h"
#include <epoxy/gl.h>
#include <math.h>

#define TEXTURE_SIZE 360

struct rgb { float r, g, b; };
struct xy { float x, y; };

struct _Hw1AppWindow {
	GtkApplicationWindow parent_instance;

	GtkAdjustment *iterations_adjustment;
	GtkAdjustment *period_adjustment;
	GtkGLArea *draw_area;
	GtkButton *reset_button;

	guint iterations;
	GLfloat baseZoom;

	bool mousePressed;
	struct xy mouseDown;
	struct xy center, mouseDownCenter, zoom;
	struct rgb texture_data[TEXTURE_SIZE];
	GLint colorizer_period;

	/* GL objects */
	guint vao;
	guint texture;
	guint program;
	guint position_location;
	guint center_location;
	guint zoom_location;
	guint iterations_location;
	guint colorizer_location;
	guint colorizer_period_location;
};

struct _Hw1AppWindowClass {
	GtkApplicationWindowClass parent_class;
};

G_DEFINE_TYPE(Hw1AppWindow, hw1_app_window, GTK_TYPE_APPLICATION_WINDOW)

/* the vertex data is constant */
static const GLfloat vertex_data[][2] = {
	{  1.f,  1.f },
	{ -1.f,  1.f },
	{ -1.f, -1.f },
	{  1.f, -1.f },
};

static void init_buffers(Hw1AppWindow *self) {
	guint buffer;

	/* we need to create a VAO to store the other buffers */
	glGenVertexArrays(1, &self->vao);
	glBindVertexArray(self->vao);

	/* this is the VBO that holds the vertex data */
	glGenBuffers(1, &buffer);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);

	/* enable and set the position attribute */
	glEnableVertexAttribArray(self->position_location);
	glVertexAttribPointer(
			self->position_location,
			2, GL_FLOAT,
			GL_FALSE,
			sizeof(GLfloat[2]), NULL);

	/* reset the state; we will re-enable the VAO when needed */
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	/* the VBO is referenced by the VAO */
	glDeleteBuffers(1, &buffer);
}

static guint create_shader(
		int	shader_type,
		const char *source,
		GError **error,
		guint *shader_out
) {
	guint shader = glCreateShader(shader_type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	int status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE)
	{
		int log_len;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);

		char *buffer = g_malloc(log_len + 1);
		glGetShaderInfoLog(shader, log_len, NULL, buffer);

		g_set_error(
				error,
				HW1_ERROR, HW1_ERROR_SHADER_COMPILATION,
				"Compilation failure in %s shader: %s",
				shader_type == GL_VERTEX_SHADER ? "vertex" : "fragment",
				buffer
		);

		g_free(buffer);

		glDeleteShader(shader);
		shader = 0;
	}

	if (shader_out != NULL)
		*shader_out = shader;

	return shader != 0;
}

static gboolean init_shaders(Hw1AppWindow *self, GError **error) {
	GBytes *source;
	self->program = 0;
	guint vertex = 0, fragment = 0;
	self->position_location = 0;
	self->center_location = 0;
	self->zoom_location = 0;
	self->iterations_location = 0;

	/* load the vertex shader */
	source = g_resources_lookup_data("/net/ldvsoft/spbau/gl/hw1-vertex.glsl", 0, NULL);
	create_shader(GL_VERTEX_SHADER, g_bytes_get_data(source, NULL), error, &vertex);
	g_bytes_unref(source);
	if (vertex == 0)
		goto out;

	/* load the fragment shader */
	source = g_resources_lookup_data("/net/ldvsoft/spbau/gl/hw1-fragment.glsl", 0, NULL);
	create_shader(GL_FRAGMENT_SHADER, g_bytes_get_data(source, NULL), error, &fragment);
	g_bytes_unref(source);
	if (fragment == 0)
		goto out;

	/* link the vertex and fragment shaders together */
	self->program = glCreateProgram();
	glAttachShader(self->program, vertex);
	glAttachShader(self->program, fragment);
	glLinkProgram(self->program);

	int status = 0;
	glGetProgramiv(self->program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		int log_len = 0;
		glGetProgramiv(self->program, GL_INFO_LOG_LENGTH, &log_len);

		char *buffer = g_malloc(log_len + 1);
		glGetProgramInfoLog(self->program, log_len, NULL, buffer);

		g_set_error(
				error,
				HW1_ERROR, HW1_ERROR_SHADER_LINK,
				"Linking failure in program: %s", buffer
		);

		g_free(buffer);

		glDeleteProgram(self->program);
		self->program = 0;

		goto out;
	}

	/* get the location of the "position" attribute */
	self->position_location = glGetAttribLocation(self->program, "position");

	/* get the location of the uniforms */
	self->center_location = glGetUniformLocation(self->program, "center");
	self->zoom_location = glGetUniformLocation(self->program, "zoom");
	self->iterations_location = glGetUniformLocation(self->program, "iterations");
	self->colorizer_location = glGetUniformLocation(self->program, "colorizer");
	self->colorizer_period_location = glGetUniformLocation(self->program, "colorizer_period");

	/* the individual shaders can be detached and destroyed */
	glDetachShader(self->program, vertex);
	glDetachShader(self->program, fragment);

out:
	if (vertex != 0)
		glDeleteShader(vertex);
	if (fragment != 0)
		glDeleteShader(fragment);

	return self->program != 0;
}

static void hsv_to_rgb(float h, float s, float v, struct rgb *rgb) {
	float c = v * s;
	float x = c * (1 - fabsf(fmodf(h / 60, 2) - 1));
	float m = v - c;
	int i = roundf(h - fmodf(h, 60)) / 60;
	switch (i) {
		case 0: *rgb = (struct rgb) { .r = c + m, .g = x + m, .b = 0 + m } ; break;
		case 1: *rgb = (struct rgb) { .r = x + m, .g = c + m, .b = 0 + m } ; break;
		case 2: *rgb = (struct rgb) { .r = 0 + m, .g = c + m, .b = x + m } ; break;
		case 3: *rgb = (struct rgb) { .r = 0 + m, .g = x + m, .b = c + m } ; break;
		case 4: *rgb = (struct rgb) { .r = x + m, .g = 0 + m, .b = c + m } ; break;
		case 5: *rgb = (struct rgb) { .r = c + m, .g = 0 + m, .b = x + m } ; break;
	}
}

static void init_texture(Hw1AppWindow *self) {
	for (int i = 0; i != TEXTURE_SIZE; ++i) {
		float h = i * 360.0 / TEXTURE_SIZE;
		hsv_to_rgb(h, 1, 1, &self->texture_data[i]);
	}

	glGenTextures(1, &self->texture);
	glBindTexture(GL_TEXTURE_1D, self->texture);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, TEXTURE_SIZE, 0, GL_RGB, GL_FLOAT, self->texture_data);

	glGenerateMipmap(GL_TEXTURE_1D);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glBindTexture(GL_TEXTURE_1D, 0);
}

static void gl_init(Hw1AppWindow *self) {
	char *title;
	const char *renderer;

	/* we need to ensure that the GdkGLContext is set before calling GL API */
	gtk_gl_area_make_current(GTK_GL_AREA(self->draw_area));

	/* if the GtkGLArea is in an error state we don't do anything */
	if (gtk_gl_area_get_error(GTK_GL_AREA(self->draw_area)) != NULL)
		return;

	/* initialize the shaders and retrieve the program data */
	GError *error = NULL;
	if (!init_shaders(self, &error)) {
		/* set the GtkGLArea in error state, so we'll see the error message
		 * rendered inside the viewport
		 */
		gtk_gl_area_set_error(GTK_GL_AREA(self->draw_area), error);
		g_error_free(error);
		return;
	}

	/* initialize the vertex buffers */
	init_buffers(self);
	init_texture(self);

	/* set the window title */
	renderer = (char *) glGetString(GL_RENDERER);
	title = g_strdup_printf("hw1 on %s", renderer ? renderer : "Unknown");
	gtk_window_set_title(GTK_WINDOW(self), title);
	g_free(title);
}

static void gl_fini(Hw1AppWindow *self) {
	/* we need to ensure that the GdkGLContext is set before calling GL API */
	gtk_gl_area_make_current(GTK_GL_AREA(self->draw_area));

	/* skip everything if we're in error state */
	if (gtk_gl_area_get_error(GTK_GL_AREA(self->draw_area)) != NULL)
		return;

	/* destroy all the resources we created */
	if (self->vao != 0)
		glDeleteVertexArrays(1, &self->vao);
	if (self->program != 0)
		glDeleteProgram(self->program);
}

static gboolean gl_draw(Hw1AppWindow *self) {
	/* clear the viewport; the viewport is automatically resized when
	 * the GtkGLArea gets a new size allocation
	 */
	glClearColor(0.5, 0.5, 0.5, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	if (self->program && self->vao) {
		/* load our program */
		glUseProgram(self->program);

		glUniform2fv(self->center_location, 1, (GLfloat *) &self->center);
		glUniform2fv(self->zoom_location, 1, (GLfloat *) &self->zoom);
		glUniform1i(self->iterations_location, self->iterations);
		glUniform1i(self->colorizer_period_location, self->colorizer_period);
		/* WTF? */
		glUniform1i(self->colorizer_location, 0);
		glBindTexture(GL_TEXTURE_1D, self->texture);

		/* use the buffers in the VAO */
		glBindVertexArray(self->vao);

		/* draw the three vertices as a triangle */
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		/* we finished using the buffers and program */
		glBindTexture(GL_TEXTURE_1D, 0);
		glBindVertexArray(0);
		glUseProgram(0);
	}

	/* flush the contents of the pipeline */
	glFlush();

	return FALSE;
}

static void adjustment_changed(
		Hw1AppWindow *self,
		GtkAdjustment *adj
) {
	double value = gtk_adjustment_get_value(adj);

	if (adj == self->iterations_adjustment) {
		self->iterations = value;
		gtk_widget_queue_draw(GTK_WIDGET(self->draw_area));
	}
	if (adj == self->period_adjustment) {
		self->colorizer_period = value;
		gtk_widget_queue_draw(GTK_WIDGET(self->draw_area));
	}
}

static void refresh_zoom(Hw1AppWindow *self) {
	GtkAllocation alloc;
	gtk_widget_get_allocation(GTK_WIDGET(self->draw_area), &alloc);
	float ratio = (alloc.width + .0) / alloc.height;

	self->zoom = (struct xy) {
		.x = fmax(1, ratio) * self->baseZoom,
		.y = fmax(1, 1 / ratio) * self->baseZoom
	};
}

static gboolean size_changed(
		Hw1AppWindow *self,
		GtkWidget *widget,
		GdkEvent *event
) {
	(void) widget;
	(void) event;

	refresh_zoom(self);
	gtk_widget_queue_draw(GTK_WIDGET(self->draw_area));
	return false;
}

static void reset_position(
		Hw1AppWindow *self,
		GtkButton *button
) {
	if (button == self->reset_button) {
		self->center = (struct xy) { .x = -.5, .y = 0 };
		self->baseZoom = 1;
		size_changed(self, NULL, NULL);
	}
}

static gboolean mouse_down(
		Hw1AppWindow *self,
		GdkEventButton *event
) {
	self->mouseDown.x = event->x;
	self->mouseDown.y = event->y;
	self->mouseDownCenter = self->center;
	self->mousePressed = true;
	return false;
}

static gboolean mouse_up(
		Hw1AppWindow *self,
		GdkEventButton *event
) {
	(void) event;
	self->mousePressed = false;
	return false;
}

static gboolean mouse_scroll(
		Hw1AppWindow *self,
		GdkEventScroll *event
) {
	if (self->mousePressed)
		return false;
	float newZoom;
	switch (event->direction) {
		case GDK_SCROLL_UP:
			newZoom = fmax(self->baseZoom / 1.5, 1e-7);
			break;
		case GDK_SCROLL_DOWN:
			newZoom = fmin(self->baseZoom * 1.5, 2);
			break;
		default:
			return false;
	}
	GtkAllocation alloc;
	gtk_widget_get_allocation(GTK_WIDGET(self->draw_area), &alloc);
	struct xy coursorRelative = {
		.x = +(event->x + 0.0) / alloc.width  * 2 - 1,
		.y = -(event->y + 0.0) / alloc.height * 2 + 1
	};
	struct xy underCoursor = {
		.x = self->center.x + coursorRelative.x * self->zoom.x,
		.y = self->center.y + coursorRelative.y * self->zoom.y
	};
	self->baseZoom = newZoom;
	refresh_zoom(self);
	self->center = (struct xy) {
		.x = underCoursor.x - coursorRelative.x * self->zoom.x,
		.y = underCoursor.y - coursorRelative.y * self->zoom.y
	};

	gtk_widget_queue_draw(GTK_WIDGET(self->draw_area));
	return false;
}

static gboolean mouse_move(
		Hw1AppWindow *self,
		GdkEventMotion* event
) {
	GtkAllocation alloc;
	gtk_widget_get_allocation(GTK_WIDGET(self->draw_area), &alloc);
	float xMovement = -(event->x - self->mouseDown.x + (double)0.0) / alloc.width  * 2 * self->zoom.x;
	float yMovement = +(event->y - self->mouseDown.y + (double)0.0) / alloc.height * 2 * self->zoom.y;

	self->center = (struct xy) {
		.x = self->mouseDownCenter.x + xMovement,
		.y = self->mouseDownCenter.y + yMovement
	};

	gtk_widget_queue_draw(GTK_WIDGET(self->draw_area));
	return false;
}

static void hw1_app_window_class_init(Hw1AppWindowClass *klass) {
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(widget_class, "/net/ldvsoft/spbau/gl/hw1-app-window.ui");

	gtk_widget_class_bind_template_child(widget_class, Hw1AppWindow, draw_area);
	gtk_widget_class_bind_template_child(widget_class, Hw1AppWindow, iterations_adjustment);
	gtk_widget_class_bind_template_child(widget_class, Hw1AppWindow, period_adjustment);
	gtk_widget_class_bind_template_child(widget_class, Hw1AppWindow, reset_button);

	gtk_widget_class_bind_template_callback(widget_class, adjustment_changed);
	gtk_widget_class_bind_template_callback(widget_class, size_changed);
	gtk_widget_class_bind_template_callback(widget_class, reset_position);

	gtk_widget_class_bind_template_callback(widget_class, mouse_down);
	gtk_widget_class_bind_template_callback(widget_class, mouse_up);
	gtk_widget_class_bind_template_callback(widget_class, mouse_move);
	gtk_widget_class_bind_template_callback(widget_class, mouse_scroll);

	gtk_widget_class_bind_template_callback(widget_class, gl_init);
	gtk_widget_class_bind_template_callback(widget_class, gl_draw);
	gtk_widget_class_bind_template_callback(widget_class, gl_fini);
}

static void hw1_app_window_init(Hw1AppWindow *self)
{
	gtk_widget_init_template(GTK_WIDGET(self));

	self->iterations = gtk_adjustment_get_value(self->iterations_adjustment);
	self->colorizer_period = gtk_adjustment_get_value(self->period_adjustment);

	reset_position(self, self->reset_button);

	gtk_window_set_icon_name(GTK_WINDOW(self), "hw1");
}

GtkWidget *hw1_app_window_new(Hw1App *app) {
	return g_object_new(hw1_app_window_get_type(), "application", app, NULL);
}
