#include "hw4_window.hpp"
#include "hw4_error.hpp"
#include "marching_geometry.hpp"

#include <gdk/gdkkeysyms.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>
#include <random>

using CLContext = cl::Context;

using Gdk::GLContext;
using Gio::Resource;
using Glib::Bytes;
using Glib::Error;
using Glib::RefPtr;
using Glib::ustring;
using Gtk::Adjustment;
using Gtk::Application;
using Gtk::Builder;
using Gtk::ListStore;
using Gtk::Window;
using cl::Buffer;
using cl::CommandQueue;
using cl::Device;
using cl::Kernel;
using cl::Platform;
using glm::mat4;
using glm::vec3;
using glm::vec4;
using std::default_random_engine;
using std::make_unique;
using std::random_device;
using std::string;
using std::to_string;
using std::uniform_real_distribution;
using std::unique_ptr;

unique_ptr<Hw4Window> Hw4Window::create() {
	auto builder{Builder::create_from_resource("/net/ldvsoft/spbau/gl/hw4_window.ui")};
	Hw4Window *result;
	builder->get_widget_derived("Hw4Window", result);

	return unique_ptr<Hw4Window>(result);
}

[[maybe_unused]]
static void gl_check(string const &name) {
	GLuint error{glGetError()};
	if (error == GL_NO_ERROR)
		std::cout << "Check " << name << " ok" << std::endl;
	else
		std::cout << "Check " << name << " FAILED " << std::hex << error << std::endl;
}

static gboolean tick_wrapper(GtkWidget *, GdkFrameClock *clock, gpointer data) {
	static_cast<Hw4Window *>(data)->tick(gdk_frame_clock_get_frame_time(clock));
	return G_SOURCE_CONTINUE;
}

Hw4Window::Hw4Window(
	BaseObjectType *type,
	RefPtr<Builder> const &builder
):
	Window(type),
	builder(builder)
{
	builder->get_widget("draw_area", area);
	builder->get_widget("draw_area_eventbox", area_eventbox);
	builder->get_widget("animate_toggle", animate);
	builder->get_widget("display_mode_combobox", display_mode_combobox);
	builder->get_widget("reset_position_button", reset_position);
	builder->get_widget("reset_animation_button", reset_animation);
	builder->get_widget("normalize_power_button", normalize_power);

	ticker_id = gtk_widget_add_tick_callback(GTK_WIDGET(area->gobj()), tick_wrapper, this, nullptr);

	display_mode_list_store = RefPtr<ListStore>::cast_dynamic(builder->get_object("display_mode_list_store"));
	spheres_adjustment = RefPtr<Adjustment>::cast_dynamic(builder->get_object("spheres_adjustment"));
	threshold_adjustment = RefPtr<Adjustment>::cast_dynamic(builder->get_object("threshold_adjustment"));
	xresolution_adjustment = RefPtr<Adjustment>::cast_dynamic(builder->get_object("xresolution_adjustment"));
	yresolution_adjustment = RefPtr<Adjustment>::cast_dynamic(builder->get_object("yresolution_adjustment"));
	zresolution_adjustment = RefPtr<Adjustment>::cast_dynamic(builder->get_object("zresolution_adjustment"));
	color_power_adjustment = RefPtr<Adjustment>::cast_dynamic(builder->get_object("color_power_adjustment"));
	reflect_power_adjustment = RefPtr<Adjustment>::cast_dynamic(builder->get_object("reflect_power_adjustment"));
	refract_power_adjustment = RefPtr<Adjustment>::cast_dynamic(builder->get_object("refract_power_adjustment"));
	refract_index_adjustment = RefPtr<Adjustment>::cast_dynamic(builder->get_object("refract_index_adjustment"));

	area->set_required_version(3, 3);
	area->set_has_depth_buffer();
	/* options */ {
		/* marching cubes */ {
			static_assert(MARCHING_CUBES == 0);
			auto &row{*display_mode_list_store->append()};
			row.set_value<ustring>(0, "Marching cubes");
		}
		/* marching cubes */ {
			static_assert(SPHERES == 1);
			auto &row{*display_mode_list_store->append()};
			row.set_value<ustring>(0, "Spheres");
		}
		/* marching cubes */ {
			static_assert(SPHERES_WITH_CUBE == 2);
			auto &row{*display_mode_list_store->append()};
			row.set_value<ustring>(0, "Spheres (with bounding cube)");
		}

		display_mode_combobox->set_active(MARCHING_CUBES);
	}

	area->signal_realize  ().connect(sigc::mem_fun(*this, &Hw4Window::gl_init));
	area->signal_unrealize().connect(sigc::mem_fun(*this, &Hw4Window::gl_finit), false);
	area->signal_render   ().connect(sigc::mem_fun(*this, &Hw4Window::gl_render));

	area_eventbox->signal_button_press_event  ().connect(sigc::mem_fun(*this, &Hw4Window::mouse_pressed));
	area_eventbox->signal_button_release_event().connect(sigc::mem_fun(*this, &Hw4Window::mouse_pressed));
	area_eventbox->signal_motion_notify_event ().connect(sigc::mem_fun(*this, &Hw4Window::mouse_moved));

	signal_key_press_event  ().connect(sigc::mem_fun(*this, &Hw4Window::key_pressed));
	signal_key_release_event().connect(sigc::mem_fun(*this, &Hw4Window::key_released));

	animate->signal_toggled().connect(sigc::mem_fun(*this, &Hw4Window::animate_toggled));
	reset_position ->signal_clicked().connect(sigc::mem_fun(*this, &Hw4Window::reset_position_clicked));
	reset_animation->signal_clicked().connect(sigc::mem_fun(*this, &Hw4Window::reset_animation_clicked));
	normalize_power->signal_clicked().connect(sigc::mem_fun(*this, &Hw4Window::normalize_power_clicked));

	spheres_adjustment      ->signal_value_changed().connect(sigc::mem_fun(*this, &Hw4Window::spheres_changed));
	threshold_adjustment    ->signal_value_changed().connect(sigc::mem_fun(*this, &Hw4Window::geometry_changed));
	xresolution_adjustment  ->signal_value_changed().connect(sigc::mem_fun(*this, &Hw4Window::geometry_changed));
	yresolution_adjustment  ->signal_value_changed().connect(sigc::mem_fun(*this, &Hw4Window::geometry_changed));
	zresolution_adjustment  ->signal_value_changed().connect(sigc::mem_fun(*this, &Hw4Window::geometry_changed));
	color_power_adjustment  ->signal_value_changed().connect(sigc::mem_fun(*this, &Hw4Window::options_changed));
	reflect_power_adjustment->signal_value_changed().connect(sigc::mem_fun(*this, &Hw4Window::options_changed));
	refract_power_adjustment->signal_value_changed().connect(sigc::mem_fun(*this, &Hw4Window::options_changed));
	refract_index_adjustment->signal_value_changed().connect(sigc::mem_fun(*this, &Hw4Window::options_changed));

	display_mode_combobox ->signal_changed().connect(sigc::mem_fun(*this, &Hw4Window::options_changed));

	view_range = 1;

	spheres_changed();
	reset_position_clicked();
	reset_animation_clicked();
}

Hw4Window::~Hw4Window() {
	gtk_widget_remove_tick_callback(GTK_WIDGET(area->gobj()), ticker_id);
}

void Hw4Window::gl_init() {
	area->make_current();
	if (area->has_error())
		return;
	std::cout << "OpenGL:\n";
	std::cout
		<< "  Renderer: " << glGetString(GL_VENDOR) << " " << glGetString(GL_RENDERER)
		<< " (version " << glGetString(GL_VERSION) << ")\n";
	std::cout << std::flush;

	auto load_resource{[this](string const &path) {
		struct res {
			union {
				const char *data;
				const unsigned char *udata;
			};
			gsize size;
		} result;

		auto resource_bytes{Resource::lookup_data_global(path)};
		result.data = static_cast<char const *>(resource_bytes->get_data(result.size));
		return result;
	}};

	auto report_error{[this](string const &msg) -> void {
		Error error(hw4_error_quark, 0, msg);
		area->set_error(error);
		std::cout << "ERROR: " << msg << std::endl;
	}};

	/* shaders */ {
		string error_string;
		auto create_program{
			[this, &error_string, &load_resource](string const &name) -> std::unique_ptr<Program> {
				string vertex(load_resource("/net/ldvsoft/spbau/gl/" + name + "_vertex.glsl").data);
				string fragment(load_resource("/net/ldvsoft/spbau/gl/" + name + "_fragment.glsl").data);
				return Program::build_program(
					{{GL_VERTEX_SHADER, vertex}, {GL_FRAGMENT_SHADER, fragment}},
					error_string
				);
			}
		};

		if ((gl.marching_program = create_program("marching")) == nullptr) {
			report_error("Program Marching cubes: " + error_string);
			return;
		}
		if ((gl.spheres_program = create_program("spheres")) == nullptr) {
			report_error("Program Marching cubes: " + error_string);
			return;
		}
		if ((gl.skybox_program = create_program("skybox")) == nullptr) {
			report_error("Program Marching cubes: " + error_string);
			return;
		}
	}

	/* framebuffer */ {
		glGenFramebuffers(1, &gl.framebuffer);
	}

	/* sphere */ {
		::Object obj{::Object::load(load_resource("/net/ldvsoft/spbau/gl/sphere.obj").data)};
		gl.sphere = make_unique<SceneObject>(obj);
	}

	/* cube */ {
		::Object obj{::Object::load(load_resource("/net/ldvsoft/spbau/gl/cube.obj").data)};
		gl.cube = make_unique<SceneObject>(obj);
		gl.cube->position = glm::scale(glm::vec3(view_range, view_range, view_range));
		gl.skybox = make_unique<SceneObject>(obj);
	}

	/* skybox */ {
		glGenTextures(1, &gl.skybox_texture);
		glBindTexture(GL_TEXTURE_CUBE_MAP, gl.skybox_texture);
		for (int i{0}; i < 6; ++i) {
			int w, h, n;
			auto img{load_resource("/net/ldvsoft/spbau/gl/skybox-" + to_string(i) + ".png")};
			stbi_uc *data = stbi_load_from_memory(img.udata, img.size, &w, &h, &n, 3);
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
				GL_RGB, w, h,
				0, GL_RGB, GL_UNSIGNED_BYTE, data);
			stbi_image_free(data);
		}
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	}

	/* marching: geometry */ try {
		/* context */ {
			Platform platform = Platform::getDefault();

			std::vector<Device> devices;
			platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);
			cl.device = devices[0];

			cl.context = CLContext(cl.device, nullptr, nullptr, nullptr);
			cl.queue = CommandQueue(cl.context, cl.device, 0);

			std::cout << "OpenCL:\n";
			std::cout
				<< "  Platform: " << platform.getInfo<CL_PLATFORM_NAME>()
				<< " (version " << platform.getInfo<CL_PLATFORM_VERSION>() << ")\n";
			std::cout
				<< "  Device  : " << cl.device.getInfo<CL_DEVICE_VENDOR>() << " " << cl.device.getInfo<CL_DEVICE_NAME>()
				<< " (version " << cl.device.getInfo<CL_DEVICE_VERSION>() << ")\n";
			std::cout << std::flush;
		}

		/* marching geometry */ {
			string kernel(load_resource("/net/ldvsoft/spbau/gl/marching_geometry.cl").data);
			cl::Program program(cl.context, kernel);
			try {
				program.build();
			} catch (cl::Error const &e) {
				report_error("OpenCL build: " + program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(cl.device));
				return;
			}

			cl.fill_values = Kernel(program, "fill_values");
			cl.find_edges = Kernel(program, "find_edges");
			cl.put_vertices = Kernel(program, "put_vertices");
			cl.build_mesh = Kernel(program, "build_mesh");
		}
	} catch (cl::Error const &e) {
		report_error(string("OpenCL: ") + e.what() + ": " + to_string(e.err()));
	}
}

void Hw4Window::gl_finit() {
	area->make_current();
	if (area->has_error())
		return;
	glDeleteFramebuffers(1, &gl.framebuffer);
	gl.skybox = nullptr;
	gl.mesh = nullptr;
	gl.cube = nullptr;
	gl.sphere = nullptr;
	gl.spheres_program = nullptr;
	gl.marching_program = nullptr;
}

bool Hw4Window::gl_render(RefPtr<GLContext> const &context) {
	static_cast<void>(context);

	if (area->has_error())
		return false;

	GLsizei height{area->get_height()}, width{area->get_width()};
	GLint old_buffer, old_vp[4];
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_buffer);
	glGetIntegerv(GL_VIEWPORT, old_vp);

	/* animate */ {
		for (auto &sphere: gl.spheres) {
			double angle(fmod(animation.progress * sphere.speed, 2 * M_PI));
			double a{cos(angle) * M_PI / 6};
			double b{angle * 3};
			sphere.position = vec3(
				sphere.radius * cos(a) * sin(b),
				sphere.radius * sin(a),
				sphere.radius * cos(a) * cos(b)
			);
		}
	}

	mat4 cam_proj{glm::perspective(
		/* vertical fov = */ glm::radians(gl.fov),
		/* ratio        = */ static_cast<float>(area->get_width()) / area->get_height(),
		/* planes       : */ .005f, 100.0f
	)};

	glClearColor(0, 0, 0, 1);

	/* buffer */ if(false) {
		glBindFramebuffer(GL_FRAMEBUFFER, gl.framebuffer);
		glViewport(0, 0, width, height);

		/* buffers */ {
			constexpr size_t cnt{0};
			GLenum buffers[cnt]{};
			glDrawBuffers(cnt, buffers);
		}
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			area->set_error(Error(hw4_error_quark, 0, "Failed to create framebuffer."));
			return false;
		}

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glBindFramebuffer(GL_FRAMEBUFFER, old_buffer);
		glViewport(old_vp[0], old_vp[1], old_vp[2], old_vp[3]);
	}

	static constexpr bool useDebug{false};
	GLuint tmp, out;
	if (useDebug) {
		glGenTextures(1, &out);
		glBindTexture(GL_TEXTURE_2D, out);
		glTexImage2D(
			GL_TEXTURE_2D, 0,
			GL_RGB16F, area->get_width(), area->get_height(),
			0, GL_RGB, GL_FLOAT, nullptr
		);
		glBindTexture(GL_TEXTURE_2D, 0);

		glGenFramebuffers(1, &tmp);
		glBindFramebuffer(GL_FRAMEBUFFER, tmp);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, out, 0);
		{
			constexpr size_t cnt{1};
			constexpr GLenum buffers[cnt]{GL_COLOR_ATTACHMENT0};
			glDrawBuffers(cnt, buffers);
		}
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			area->set_error(Error(hw4_error_quark, 0, "TMP"));
			return false;
		}
	}

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	switch (display_mode_combobox->get_active_row_number()) {
	case MARCHING_CUBES:
		gl_render_marching(get_camera_view(), cam_proj);
		gl_render_skybox(get_camera_view(), cam_proj);
		break;
	case SPHERES:
		gl_render_spheres(get_camera_view(), cam_proj, false);
		gl_render_skybox(get_camera_view(), cam_proj);
		break;
	case SPHERES_WITH_CUBE:
		gl_render_spheres(get_camera_view(), cam_proj, true);
		gl_render_skybox(get_camera_view(), cam_proj);
		break;
	}

	if (useDebug) {
		glBindFramebuffer(GL_FRAMEBUFFER, old_buffer);
		glDeleteTextures(1, &out);
		glDeleteFramebuffers(1, &tmp);
	}

	glFlush();

	return false;
}

template<typename T>
void cl_read_buffer(std::vector<T> &target, Buffer const &buffer, CommandQueue const &queue) {
	assert(sizeof(T) * target.size() == buffer.getInfo<CL_MEM_SIZE>());
	queue.enqueueReadBuffer(buffer, false, 0, sizeof(T) * target.size(), target.data());
}

template<typename T>
void cl_write_buffer(std::vector<T> &target, Buffer const &buffer, CommandQueue const &queue) {
	assert(sizeof(T) * target.size() == buffer.getInfo<CL_MEM_SIZE>());
	queue.enqueueWriteBuffer(buffer, false, 0, sizeof(T) * target.size(), target.data());
}

template<typename T>
void cl_fill_buffer(T const &value, Buffer const &buffer, CommandQueue const &queue) {
	queue.enqueueFillBuffer<T>(buffer, value, 0, buffer.getInfo<CL_MEM_SIZE>());
}

void Hw4Window::gl_render_marching(mat4 const &view, mat4 const &proj) {
	auto report_error{[this](string const &msg) -> void {
		Error error(hw4_error_quark, 0, msg);
		area->set_error(error);
		std::cout << "ERROR: " << msg << std::endl;
	}};

	/* calculations */ if (gl.mesh == nullptr) try {
		/* HERE AND BELOW:
		 * OpenCL treats float3 like float4 inside,
		 * but here we use GLM types...
		 * DO NOT USE cl_float3
		 */
		using cl_vec3 = vec4;

		cl_int
			n(xresolution_adjustment->get_value()),
			m(yresolution_adjustment->get_value()),
			k(zresolution_adjustment->get_value());

		cl_int const
			vertices{(n + 1) * (m + 1) * (k + 1)},
			edges{vertices * 3},
			cubes{n * m * k};

		Buffer
			values_buffer(cl.context, CL_MEM_READ_WRITE, sizeof(cl_uint) * vertices),
			a_buffer(cl.context, CL_MEM_READ_ONLY, sizeof(cl_float) * gl.spheres.size()),
			c_buffer(cl.context, CL_MEM_READ_ONLY, sizeof(cl_vec3) * gl.spheres.size()),
			edge_used_buffer(cl.context, CL_MEM_WRITE_ONLY, sizeof(cl_int) * edges);

		/* fill data */ {
			std::vector<float> a;
			std::vector<cl_vec3> c;
			for (auto const &sphere: gl.spheres) {
				a.push_back(sphere.power);
				c.push_back(cl_vec3(sphere.position, 0)); // (<_<)
			}

			cl_write_buffer(a, a_buffer, cl.queue);
			cl_write_buffer(c, c_buffer, cl.queue);
			cl_fill_buffer<cl_int>(0, edge_used_buffer, cl.queue);
		}

		cl.fill_values.setArg(0, n);
		cl.fill_values.setArg(1, m);
		cl.fill_values.setArg(2, k);
		cl.fill_values.setArg<float>(3, view_range);
		cl.fill_values.setArg<int>(4, gl.spheres.size());
		cl.fill_values.setArg(5, a_buffer);
		cl.fill_values.setArg(6, c_buffer);
		cl.fill_values.setArg<float>(7, threshold_adjustment->get_value());
		cl.fill_values.setArg(8, values_buffer);

		cl.queue.enqueueNDRangeKernel(cl.fill_values, cl::NullRange, cl::NDRange(n + 1, m + 1, k + 1), cl::NullRange);

		cl.find_edges.setArg(0, n);
		cl.find_edges.setArg(1, m);
		cl.find_edges.setArg(2, k);
		cl.find_edges.setArg(3, values_buffer);
		cl.find_edges.setArg(4, edge_used_buffer);

		cl.queue.enqueueNDRangeKernel(cl.find_edges, cl::NullRange, cl::NDRange(n + 1, m + 1, k + 1), cl::NullRange);

		std::vector<int> edge_used(edges);
		std::vector<float> values(vertices);
		cl_read_buffer(edge_used, edge_used_buffer, cl.queue);
		cl_read_buffer(values, values_buffer, cl.queue);
		cl.queue.finish();

		Buffer
			vertex_ids_buffer(cl.context, CL_MEM_READ_ONLY, sizeof(cl_int) * edges);
		std::vector<int> vertex_ids(edges, -2);
		int vertex_count{0};
		/* fill ids */ {
			int v_line{n + 1};
			int v_plane{v_line * (m + 1)};

			for (int z{0}; z <= k; ++z)
				for (int y{0}; y <= m; ++y)
					for (int x{0}; x <= n; ++x) {
						int v_id{z * v_plane + y * v_line + x};
						if (x < n && edge_used[v_id * 3 + 0] == 1)
							vertex_ids[v_id * 3 + 0] = vertex_count++;
						if (y < m && edge_used[v_id * 3 + 1] == 1)
							vertex_ids[v_id * 3 + 1] = vertex_count++;
						if (z < k && edge_used[v_id * 3 + 2] == 1)
							vertex_ids[v_id * 3 + 2] = vertex_count++;
					}
			cl_write_buffer(vertex_ids, vertex_ids_buffer, cl.queue);
		}
		if (vertex_count == 0)
			return;
		Buffer
			vertex_pos_buffer(cl.context, CL_MEM_READ_WRITE, sizeof(cl_vec3) * vertex_count),
			vertex_norm_buffer(cl.context, CL_MEM_WRITE_ONLY, sizeof(cl_vec3) * vertex_count);

		cl.put_vertices.setArg(0, n);
		cl.put_vertices.setArg(1, m);
		cl.put_vertices.setArg(2, k);
		cl.put_vertices.setArg<float>(3, view_range);
		cl.put_vertices.setArg<int>(4, gl.spheres.size());
		cl.put_vertices.setArg(5, a_buffer);
		cl.put_vertices.setArg(6, c_buffer);
		cl.put_vertices.setArg(7, values_buffer);
		cl.put_vertices.setArg(8, vertex_ids_buffer);
		cl.put_vertices.setArg(9, vertex_pos_buffer);
		cl.put_vertices.setArg(10, vertex_norm_buffer);

		cl.queue.enqueueNDRangeKernel(cl.put_vertices, cl::NullRange, cl::NDRange(n + 1, m + 1, k + 1), cl::NullRange);

		Buffer
			triangles_buffer(cl.context, CL_MEM_WRITE_ONLY, sizeof(cl_int) * cubes * MAX_TRIANGLES * 3),
			cases_buffer(cl.context, CL_MEM_WRITE_ONLY, sizeof(cl_int) * cubes);
		cl_fill_buffer<cl_int>(-1, triangles_buffer, cl.queue);

		cl.build_mesh.setArg(0, n);
		cl.build_mesh.setArg(1, m);
		cl.build_mesh.setArg(2, k);
		cl.build_mesh.setArg<float>(3, view_range);
		cl.build_mesh.setArg(4, values_buffer);
		cl.build_mesh.setArg(5, vertex_ids_buffer);
		cl.build_mesh.setArg(6, vertex_pos_buffer);
		cl.build_mesh.setArg(7, triangles_buffer);

		cl.queue.enqueueNDRangeKernel(cl.build_mesh, cl::NullRange, cl::NDRange(n, m, k), cl::NullRange);

		std::vector<cl_vec3>
			vertex_pos(vertex_count),
			vertex_norm(vertex_count);
		std::vector<int> triangles(cubes * MAX_TRIANGLES * 3, -1);

		/* debug */
		std::vector<int> cases(cubes);
		cl_read_buffer(cases, cases_buffer, cl.queue);

		cl_read_buffer(vertex_pos, vertex_pos_buffer, cl.queue);
		cl_read_buffer(vertex_norm, vertex_norm_buffer, cl.queue);
		cl_read_buffer(triangles, triangles_buffer, cl.queue);
		cl.queue.finish();

		std::vector<::Object::vertex_data> object_data(vertex_count);
		std::vector<glm::uvec3> object_elems;
		for (int i{0}; i < cubes * MAX_TRIANGLES; ++i) {
			if (triangles.at(3 * i) == -1)
				continue;
			object_elems.emplace_back(triangles.at(3 * i), triangles.at(3 * i + 1), triangles.at(3 * i + 2));
			for (int j{0}; j < 3; ++j) {
				int id{triangles[3 * i + j]};
				if (id < 0) {
					std::cerr << "AT POSITION " << i << ":" << j << " GOT " << id << std::endl;
					continue;
				}
			}
		}
		for (int i{0}; i < vertex_count; ++i) {
			object_data.at(i).pos = vertex_pos.at(i);
			object_data.at(i).norm = vertex_norm.at(i);
		}

		gl.mesh = make_unique<SceneObject>(::Object::manual(object_data, object_elems));
	} catch (cl::Error const &e) {
		report_error(string("OpenCL: ") + e.what() + ": " + to_string(e.err()));
		return;
	}

	/* rrrender */ {
		gl.marching_program->use();

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, gl.skybox_texture);

		glUniform1i(gl.marching_program->get_uniform("skybox"), 0);
		glUniform1f(gl.marching_program->get_uniform("color_power"), color_power_adjustment->get_value());
		glUniform1f(gl.marching_program->get_uniform("reflect_power"), reflect_power_adjustment->get_value());
		glUniform1f(gl.marching_program->get_uniform("refract_power"), refract_power_adjustment->get_value());
		glUniform1f(gl.marching_program->get_uniform("refract_index"), refract_index_adjustment->get_value());
		glUniform3fv(gl.marching_program->get_uniform("eye_world"), 1, &navigation.camera_position[0]);
		gl_draw_object(*gl.mesh, *gl.marching_program, view, proj);

		glUseProgram(0);
	}
}

void Hw4Window::gl_render_spheres(mat4 const &view, mat4 const &proj, bool box) {
	gl.spheres_program->use();

	glUniform1i(gl.spheres_program->get_uniform("mode"), 0);
	double threshold{threshold_adjustment->get_value()};
	for (auto const &sphere: gl.spheres) {
		double rad{sqrt(threshold / sphere.power)};
		gl.sphere->position = glm::scale(
			glm::translate(sphere.position),
			vec3(1 / rad, 1 / rad, 1 / rad)
		);
		gl_draw_object(*gl.sphere, *gl.spheres_program, view, proj);
	}

	if (box) {
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);

		glUniform1i(gl.spheres_program->get_uniform("mode"), 1);
		gl_draw_object(*gl.cube, *gl.spheres_program, view, proj);

		glDisable(GL_CULL_FACE);
	}

	glUseProgram(0);
}

void Hw4Window::gl_render_skybox(mat4 const &view, mat4 const &proj) {
	gl.skybox_program->use();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, gl.skybox_texture);

	glUniform1f(gl.skybox_program->get_uniform("size"), 10);
	glUniform1i(gl.skybox_program->get_uniform("skybox"), 0);

	gl.skybox->animation_position = glm::translate(navigation.camera_position);
	gl_draw_object(*gl.skybox, *gl.skybox_program, view, proj);

	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

	glUseProgram(0);
}

void Hw4Window::gl_draw_object(
	SceneObject const &object,
	Program const &program,
	mat4 const &v, mat4 const &p
) {
	object.set_attribute_to_position(program.get_attribute("vertex_position_model"));
	object.set_attribute_to_normal  (program.get_attribute("vertex_normal_model"));
	object.set_attribute_to_color   (program.get_attribute("vertex_color"));
	glm::mat4 vp(p * v);
	glm::mat4 v_inv(glm::inverse(v)), p_inv(glm::inverse(p)), vp_inv(glm::inverse(vp));
	glUniformMatrix4fv(program.get_uniform("v"     ), 1, GL_FALSE, &v[0][0]);
	glUniformMatrix4fv(program.get_uniform("p"     ), 1, GL_FALSE, &p[0][0]);
	glUniformMatrix4fv(program.get_uniform("vp"    ), 1, GL_FALSE, &vp[0][0]);
	glUniformMatrix4fv(program.get_uniform("v_inv" ), 1, GL_FALSE, &v_inv[0][0]);
	glUniformMatrix4fv(program.get_uniform("p_inv" ), 1, GL_FALSE, &p_inv[0][0]);
	glUniformMatrix4fv(program.get_uniform("vp_inv"), 1, GL_FALSE, &vp_inv[0][0]);
	object.draw(
		v, p,
		program.get_uniform("m"),
		program.get_uniform("mv"),
		program.get_uniform("mvp"),
		program.get_uniform("m_inv"),
		program.get_uniform("mv_inv"),
		program.get_uniform("mvp_inv")
	);
}

mat4 Hw4Window::get_camera_view() const {
	return glm::rotate(navigation.yangle, vec3(1, 0, 0)) *
		glm::rotate(navigation.xangle, vec3(0, -1, 0)) *
		glm::translate(-navigation.camera_position);
}

void Hw4Window::animate_toggled() {
	bool state{animate->get_active()};
	if (state) {
		animation.state = animation.PENDING;
	} else {
		animation.state = animation.STOPPED;
	}
}

void Hw4Window::tick(gint64 new_time) {
	gint64 delta{new_time - animation.start_time};
	double seconds_delta{delta / static_cast<double>(G_USEC_PER_SEC)};
	/* animation */ {
		switch (animation.state) {
		case animation.PENDING:
			animation.start_time = new_time;
			animation.start_progress = animation.progress;
			animation.state = animation.STARTED;
			break;
		case animation.STARTED:
			animation.progress = animation.start_progress + seconds_delta * animation.progress_per_second;
			geometry_changed();
			break;
		default:
			break;
		}
	}
	/* navigation */ {
		bool flag{false};
		vec3 direction(0, 0, 0);
		if (navigation.to_left) {
			direction += vec3(-1, 0, 0);
			flag = true;
		}
		if (navigation.to_right) {
			direction += vec3(+1, 0, 0);
			flag = true;
		}
		if (navigation.to_up) {
			direction += vec3(0, +1, 0);
			flag = true;
		}
		if (navigation.to_down) {
			direction += vec3(0, -1, 0);
			flag = true;
		}
		if (navigation.to_front) {
			direction += vec3(0, 0, -1);
			flag = true;
		}
		if (navigation.to_back) {
			direction += vec3(0, 0, +1);
			flag = true;
		}
		if (flag) {
			direction *= view_range / 20;
			navigation.camera_position += vec3(inverse(get_camera_view()) * glm::vec4(direction, 0));
			options_changed();
		}
	}
}

void Hw4Window::reset_position_clicked() {
	navigation.xangle = 0;
	navigation.yangle = 0;
	navigation.camera_position = vec3(0, 0, view_range * (1 + 1 / tan((gl.fov * M_PI / 180) / 2)));
	options_changed();
}

void Hw4Window::reset_animation_clicked() {
	animation.progress = 0;
	if (animation.state == animation.STARTED)
		animation.state = animation.PENDING;
	geometry_changed();
}

void Hw4Window::normalize_power_clicked() {
	double value{0};
	value += color_power_adjustment  ->get_value();
	value += reflect_power_adjustment->get_value();
	value += refract_power_adjustment->get_value();
	color_power_adjustment  ->set_value(color_power_adjustment->get_value()   / value);
	reflect_power_adjustment->set_value(reflect_power_adjustment->get_value() / value);
	refract_power_adjustment->set_value(refract_power_adjustment->get_value() / value);
	options_changed();
}

void Hw4Window::spheres_changed() {
	size_t new_size(spheres_adjustment->get_value()), old_size{gl.spheres.size()};
	gl.spheres.resize(new_size);
	for (size_t i{old_size}; i < new_size; ++i) {
		random_device rd;
		default_random_engine rand(rd());
		uniform_real_distribution<double> dist(-1, 1);
		gl.spheres[i].power  = dist(rand) * .02 + .09;
		gl.spheres[i].speed  = dist(rand) * 8 + 15;
		gl.spheres[i].radius = dist(rand) * .1 + .5;
	}
	geometry_changed();
}

void Hw4Window::geometry_changed() {
	gl.mesh = nullptr;
	options_changed();
}

void Hw4Window::options_changed() {
	area->queue_render();
}

bool Hw4Window::mouse_pressed(GdkEventButton *event) {
	navigation.pressed = true;
	navigation.start_xangle = navigation.xangle;
	navigation.start_yangle = navigation.yangle;
	navigation.start_x = event->x;
	navigation.start_y = event->y;
	return false;
}

bool Hw4Window::mouse_released(GdkEventButton *event) {
	static_cast<void>(event);
	navigation.pressed = false;
	return false;
}

bool Hw4Window::mouse_moved(GdkEventMotion *event) {
	if (!navigation.pressed)
		return false;

	//double vfov{gl.fov / 2}, hfov{atan(tan(gl.fov) * area->get_width() / area->get_height()) / 2};
	double rel_x{-1 + 2 * event->x / area->get_width()};
	double rel_y{+1 - 2 * event->y / area->get_height()};
	double rel_start_x{-1 + 2 * navigation.start_x / area->get_width()};
	double rel_start_y{+1 - 2 * navigation.start_y / area->get_height()};

	double dx{rel_x - rel_start_x};
	double dy{rel_y - rel_start_y};

	navigation.xangle = fmod(navigation.start_xangle + dx, 2 * M_PI);
	navigation.yangle = glm::clamp(navigation.start_yangle + dy, -M_PI / 2, +M_PI / 2);

	area->queue_render();
	return false;
}

bool Hw4Window::key_pressed(GdkEventKey *event) {
	switch (event->keyval) {
	case GDK_KEY_a:
		navigation.to_left = true;
		break;
	case GDK_KEY_d:
		navigation.to_right = true;
		break;
	case GDK_KEY_r:
		navigation.to_up = true;
		break;
	case GDK_KEY_f:
		navigation.to_down = true;
		break;
	case GDK_KEY_w:
		navigation.to_front = true;
		break;
	case GDK_KEY_s:
		navigation.to_back = true;
		break;
	}
	return false;
}

bool Hw4Window::key_released(GdkEventKey *event) {
	switch (event->keyval) {
	case GDK_KEY_a:
		navigation.to_left = false;
		break;
	case GDK_KEY_d:
		navigation.to_right = false;
		break;
	case GDK_KEY_r:
		navigation.to_up = false;
		break;
	case GDK_KEY_f:
		navigation.to_down = false;
		break;
	case GDK_KEY_w:
		navigation.to_front = false;
		break;
	case GDK_KEY_s:
		navigation.to_back = false;
		break;
	}
	return false;
}
