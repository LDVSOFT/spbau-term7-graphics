#include "glarea-app-window.h"
#include "glarea-error.h"
#include <epoxy/gl.h>
#include <math.h>

struct _GlareaAppWindow {
	GtkApplicationWindow parent_instance;

	GtkAdjustment *iterations_adjustment;
	GtkGLArea *drawArea;
	GtkButton *reset_button;

	guint iterations;
	GLfloat baseZoom;

	bool mousePressed;
	struct { int x; int y; } mouseDown;
	GLfloat center[2], mouseDownCenter[2], zoom[2];

	/* GL objects */
	guint vao;
	guint program;
	guint position_index;
	guint center_location;
	guint zoom_location;
	guint iterations_location;
};

struct _GlareaAppWindowClass {
	GtkApplicationWindowClass parent_class;
};

G_DEFINE_TYPE(GlareaAppWindow, glarea_app_window, GTK_TYPE_APPLICATION_WINDOW)

/* the vertex data is constant */
static const GLfloat vertex_data[][2] = {
	{  1.f,  1.f },
	{ -1.f,  1.f },
	{ -1.f, -1.f },
	{  1.f, -1.f },
};

static void init_buffers(
		guint position_index,
		guint *vao_out
) {
	guint vao, buffer;

	/* we need to create a VAO to store the other buffers */
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	/* this is the VBO that holds the vertex data */
	glGenBuffers(1, &buffer);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);

	/* enable and set the position attribute */
	glEnableVertexAttribArray(position_index);
	glVertexAttribPointer(
			position_index, 
			2, GL_FLOAT, 
			GL_FALSE,
			sizeof(GLfloat[2]), NULL);

	/* reset the state; we will re-enable the VAO when needed */
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	/* the VBO is referenced by the VAO */
	glDeleteBuffers(1, &buffer);

	if (vao_out != NULL)
		*vao_out = vao;
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
				GLAREA_ERROR, GLAREA_ERROR_SHADER_COMPILATION,
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

static gboolean init_shaders(
		guint *program_out,
		guint *position_location_out,
		guint *center_location_out,
		guint *zoom_location_out,
		guint *iterations_location_out,
		GError **error
) {
	GBytes *source;
	guint program = 0;
	guint vertex = 0, fragment = 0;
	guint position_location = 0;
	guint center_location = 0;
	guint zoom_location = 0;
	guint iterations_location = 0;

	/* load the vertex shader */
	source = g_resources_lookup_data("/io/bassi/glarea/glarea-vertex.glsl", 0, NULL);
	create_shader(GL_VERTEX_SHADER, g_bytes_get_data(source, NULL), error, &vertex);
	g_bytes_unref(source);
	if (vertex == 0)
		goto out;

	/* load the fragment shader */
	source = g_resources_lookup_data("/io/bassi/glarea/glarea-fragment.glsl", 0, NULL);
	create_shader(GL_FRAGMENT_SHADER, g_bytes_get_data(source, NULL), error, &fragment);
	g_bytes_unref(source);
	if (fragment == 0)
		goto out;

	/* link the vertex and fragment shaders together */
	program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glLinkProgram(program);

	int status = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		int log_len = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);

		char *buffer = g_malloc(log_len + 1);
		glGetProgramInfoLog(program, log_len, NULL, buffer);

		g_set_error(
				error, 
				GLAREA_ERROR, GLAREA_ERROR_SHADER_LINK,
				"Linking failure in program: %s", buffer
		);

		g_free(buffer);

		glDeleteProgram(program);
		program = 0;

		goto out;
	}

	/* get the location of the "position" attribute */
	position_location = glGetAttribLocation(program, "position");

	/* get the location of the uniforms */
	center_location = glGetUniformLocation(program, "center");
	zoom_location = glGetUniformLocation(program, "zoom");
	iterations_location = glGetUniformLocation(program, "iterations");

	/* the individual shaders can be detached and destroyed */
	glDetachShader(program, vertex);
	glDetachShader(program, fragment);

out:
	if (vertex != 0)
		glDeleteShader(vertex);
	if (fragment != 0)
		glDeleteShader(fragment);

	if (program_out != NULL)
		*program_out = program;
	if (position_location_out != NULL)
		*position_location_out = position_location;
	if (center_location_out != NULL)
		*center_location_out = center_location;
	if (zoom_location_out != NULL)
		*zoom_location_out = zoom_location;
	if (iterations_location_out != NULL)
		*iterations_location_out = iterations_location;

	return program != 0;
}

static void gl_init(GlareaAppWindow *self) {
	char *title;
	const char *renderer;

	/* we need to ensure that the GdkGLContext is set before calling GL API */
	gtk_gl_area_make_current(GTK_GL_AREA(self->drawArea));

	/* if the GtkGLArea is in an error state we don't do anything */
	if (gtk_gl_area_get_error(GTK_GL_AREA(self->drawArea)) != NULL)
		return;

	/* initialize the shaders and retrieve the program data */
	GError *error = NULL;
	if (!init_shaders(
				&self->program,
				&self->position_index,
				&self->center_location,
				&self->zoom_location,
				&self->iterations_location,
				&error
		)
	) {
		/* set the GtkGLArea in error state, so we'll see the error message
		 * rendered inside the viewport
		 */
		gtk_gl_area_set_error(GTK_GL_AREA(self->drawArea), error);
		g_error_free(error);
		return;
	}

	/* initialize the vertex buffers */
	init_buffers(self->position_index, &self->vao);

	/* set the window title */
	renderer = (char *) glGetString(GL_RENDERER);
	title = g_strdup_printf("glarea on %s", renderer ? renderer : "Unknown");
	gtk_window_set_title(GTK_WINDOW(self), title);
	g_free(title);
}

static void gl_fini(GlareaAppWindow *self) {
	/* we need to ensure that the GdkGLContext is set before calling GL API */
	gtk_gl_area_make_current(GTK_GL_AREA(self->drawArea));

	/* skip everything if we're in error state */
	if (gtk_gl_area_get_error(GTK_GL_AREA(self->drawArea)) != NULL)
		return;

	/* destroy all the resources we created */
	if (self->vao != 0)
		glDeleteVertexArrays(1, &self->vao);
	if (self->program != 0)
		glDeleteProgram(self->program);
}

static gboolean gl_draw(GlareaAppWindow *self) {
	/* clear the viewport; the viewport is automatically resized when
	 * the GtkGLArea gets a new size allocation
	 */
	glClearColor(0.5, 0.5, 0.5, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	if (self->program && self->vao) {
		/* load our program */
		glUseProgram(self->program);

		glUniform2fv(self->center_location, 1, self->center);
		glUniform2fv(self->zoom_location, 1, self->zoom);
		glUniform1i(self->iterations_location, self->iterations);

		/* use the buffers in the VAO */
		glBindVertexArray(self->vao);

		/* draw the three vertices as a triangle */
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		/* we finished using the buffers and program */
		glBindVertexArray(0);
		glUseProgram(0);
	}

	/* flush the contents of the pipeline */
	glFlush();

	return FALSE;
}

static void adjustment_changed(
		GlareaAppWindow *self,
		GtkAdjustment *adj
) {
	double value = gtk_adjustment_get_value(adj);

	if (adj == self->iterations_adjustment) {
		self->iterations = value;
		gtk_widget_queue_draw(GTK_WIDGET(self->drawArea));
	}
}

static gboolean size_changed(
		GlareaAppWindow *self,
		GtkWidget *widget,
		GdkEvent *event
) {
	(void) widget;
	(void) event;

	GtkAllocation alloc;
	gtk_widget_get_allocation(GTK_WIDGET(self->drawArea), &alloc);
	float ratio = (alloc.width + .0) / alloc.height;

	self->zoom[0] = fmax(1, ratio) * self->baseZoom;
	self->zoom[1] = fmax(1, 1 / ratio) * self->baseZoom;
	
	gtk_widget_queue_draw(GTK_WIDGET(self->drawArea));
	return false;
}

static void reset_position(
		GlareaAppWindow *self,
		GtkButton *button
) {
	if (button == self->reset_button) {
		self->center[0] = -.5;
	   	self->center[1] = 0;
		self->baseZoom = 1;
		size_changed(self, NULL, NULL);
	}
}

static gboolean mouse_down(
		GlareaAppWindow *self,
		GdkEventButton *event
) {
	self->mouseDown.x = event->x;
	self->mouseDown.y = event->y;
	self->mouseDownCenter[0] = self->center[0];
	self->mouseDownCenter[1] = self->center[1];
	self->mousePressed = true;
	return false;
}

static gboolean mouse_up(
		GlareaAppWindow *self,
		GdkEventButton *event
) {
	self->mousePressed = false;
	return false;
}

static gboolean mouse_scroll(
		GlareaAppWindow *self,
		GdkEventScroll *event
) {
	if (self->mousePressed)
		return false;
	float old = self->baseZoom;
	switch (event->direction) {
		case GDK_SCROLL_UP:
			self->baseZoom = fmax(self->baseZoom /= 2, 1e-7);
			break;
		case GDK_SCROLL_DOWN:
			self->baseZoom = fmin(self->baseZoom *= 2, 2);
			break;
	}
	size_changed(self, NULL, NULL);
	return false;
}

static gboolean mouse_move(
		GlareaAppWindow *self,
		GdkEventMotion* event
) {
	GtkAllocation alloc;
	alloc.x = gtk_widget_get_allocated_width (GTK_WIDGET(self->drawArea));
	alloc.y = gtk_widget_get_allocated_height(GTK_WIDGET(self->drawArea));
	float xMovement = -(event->x - self->mouseDown.x + (double)0.0) / alloc.x * 2 * self->zoom[0];
	float yMovement = +(event->y - self->mouseDown.y + (double)0.0) / alloc.y * 2 * self->zoom[1];

	self->center[0] = self->mouseDownCenter[0] + xMovement;
	self->center[1] = self->mouseDownCenter[1] + yMovement;

	gtk_widget_queue_draw(GTK_WIDGET(self->drawArea));
	return false;
}

static void glarea_app_window_class_init(GlareaAppWindowClass *klass) {
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(widget_class, "/io/bassi/glarea/glarea-app-window.ui");

	gtk_widget_class_bind_template_child(widget_class, GlareaAppWindow, drawArea);
	gtk_widget_class_bind_template_child(widget_class, GlareaAppWindow, iterations_adjustment);
	gtk_widget_class_bind_template_child(widget_class, GlareaAppWindow, reset_button);

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

static void glarea_app_window_init(GlareaAppWindow *self)
{
	gtk_widget_init_template(GTK_WIDGET(self));

	self->iterations = gtk_adjustment_get_value(self->iterations_adjustment);
	
	reset_position(self, self->reset_button);

	gtk_window_set_icon_name(GTK_WINDOW(self), "glarea");
}

GtkWidget *glarea_app_window_new(GlareaApp *app) {
	return g_object_new(glarea_app_window_get_type(), "application", app, NULL);
}
