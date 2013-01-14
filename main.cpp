// Standard libraries
#include <cstdio>
#include <cstdlib>

#include <signal.h>

#include <iostream>
#include <sstream>

#if defined _WIN32
	#include <windows.h>
#elif defined __linux__
	#include <GL/glx.h>
#endif

#include <ctime>
#include <cmath>

// OpenCL libraries
#define __NO_STD_VECTOR // Use cl::vector instead of STL version
#include <CL/cl.h>
#define CL_USE_DEPRECATED_OPENCL_1_1_APIS
#include <CL/cl_gl.h>

// OpenGL libraries
#include <GL/glew.h>

#define GLFW_INCLUDE_GL3
#include <GL/glfw.h>

// GL math libraries
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

// Utility library
#include "utils.hpp"

// Define base values
#define WIDTH 		1024
#define HEIGHT 		WIDTH

#define CELL		8

#define FPS			120

// Static data
static const GLfloat g_vertex_buffer_data[] = {
    0.0f,			0.0f, 			0.0f, 1.0f,
    0.0f,  			HEIGHT * CELL, 	0.0f, 1.0f,
    WIDTH * CELL, 	0.0f, 			0.0f, 1.0f,
    WIDTH * CELL, 	HEIGHT * CELL, 	0.0f, 1.0f
};

static const GLfloat g_uv_buffer_data[] = {
	0.0f, 0.0f,
	0.0f, 1.0f,
	1.0f, 0.0f,
	1.0f, 1.0f
};

static const GLuint g_index_buffer_data[] = {
	0, 1, 2,
	2, 1, 3
};

// Program globals
unsigned int 		running = 1;
unsigned int 		pause = 0;
unsigned int		speed = 10;

size_t				row = 0, col = 0;
float				x_translate = 0.0f, y_translate = 0.0f;
int					mouse_x = 0, mouse_y = 0;
int					window_width = 512, window_height = 512;
unsigned int		cell_size = CELL;

GLubyte				current_tool = 0x00;

// OpenGL globals
GLuint				program;
GLuint				universe_texture_location, background_texture_location;
GLuint				view_location, perspective_location, translate_location, scale_location, frame_location;

GLuint				vao;

GLuint				universe_texture, update_texture, background_texture;

glm::mat4			view, perspective, translate, scale;

// OpenCL globals
cl_platform_id		platform;
cl_context			context;
cl_device_id 		device;
cl_command_queue 	queue;
cl_kernel			kernel;

cl_mem 				universe_buffer, update_buffer, random_buffer;

cl_mem				buffers[] = {universe_buffer, update_buffer};

cl_sampler			sampler;

/*
 * Generate initial random seed for OpenCL
 */
void randomize() {
	GLfloat *seed = new GLfloat[WIDTH * HEIGHT];

	srand(time(NULL));

	for(unsigned int i = 0; i < WIDTH * HEIGHT; i++) {
		seed[i] = rand() / (float) RAND_MAX;
	}

	clEnqueueAcquireGLObjects(queue, 2, buffers, 0, NULL, NULL);
	clEnqueueWriteBuffer(queue, random_buffer, CL_TRUE, 0, sizeof(GLfloat) * WIDTH * HEIGHT, seed, 0, NULL, NULL);
	clEnqueueReleaseGLObjects(queue, 2, buffers, 0, NULL, NULL);
	clFinish(queue);

	delete seed;
}

/*
 * Wipe all data from the universe :_D
 */
void clear() {
	const size_t origin[] = {0, 0, 0};
	const size_t region[] = {WIDTH, HEIGHT, 1};

	GLubyte *seed = new GLubyte[WIDTH * HEIGHT];

	clEnqueueAcquireGLObjects(queue, 2, buffers, 0, NULL, NULL);
	clEnqueueWriteImage(queue, universe_buffer, CL_FALSE, origin, region, 0, 0, seed, 0, NULL, NULL);
	clEnqueueReleaseGLObjects(queue, 2, buffers, 0, NULL, NULL);
	clFinish(queue);

	delete seed;
}

/*
 * Place values to the universe
 */
void touch() {
    const size_t origin[] = {col, row, 0};
    const size_t region[] = {1, 1, 1};

	clEnqueueAcquireGLObjects(queue, 2, buffers, 0, NULL, NULL);
    clEnqueueWriteImage(queue, universe_buffer, CL_FALSE, origin, region, 0, 0, &current_tool, 0, NULL, NULL);
	clEnqueueReleaseGLObjects(queue, 2, buffers, 0, NULL, NULL);
	clFinish(queue);
}

/*
 * GLFW callback functions
 */
void exit_handler(int s){
	running = 0;
}

int GLFWCALL close_handler() {
	exit_handler(0);
	return 1;
}

void GLFWCALL window_size(int width, int height) {
	glViewport(0, 0, width, height);
	float left = 0.0;
	float right = width;
	float bottom = 0.0;
	float top = height;

	perspective = glm::ortho(left, right, bottom, top);

	glUseProgram(program);
	glUniformMatrix4fv(perspective_location, 1, GL_FALSE, &perspective[0][0]);
	glUseProgram(0);

	window_width = width;
	window_height = height;
}

void GLFWCALL key_handler(int key, int action) {
	if(action == GLFW_PRESS) {
		switch(key) {
		case GLFW_KEY_ESC:
			exit_handler(0);
			break;
		case GLFW_KEY_SPACE:
			pause = !pause;
			break;
		}
	}
}

void GLFWCALL char_handler(int character, int action) {
	if(action == GLFW_PRESS) {
		switch(character) {
		case 48:
			current_tool = 0;
			break;
		case 49:
			current_tool = 19;
			break;
		case 50:
			current_tool = 29;
			break;
		case 51:
			current_tool = 39;
			break;
		case 99:
			clear();
			break;
		}
	}
}

void GLFWCALL button_handler(int button, int action) {
	if(action == GLFW_PRESS) {
		switch(button) {
		case GLFW_MOUSE_BUTTON_1:
			touch();
			break;
		}
	}
}

void GLFWCALL pos_handler(int x, int y) {
	// normalize y to bottom
	y = (window_height - 1) - y;

	// calculate actual row, column value
	size_t new_row = y / (float) cell_size - (float) y_translate / CELL;
	size_t new_col = x / (float) cell_size - (float) x_translate / CELL;
	if(new_row != row || new_col != col) {
		row = new_row;
		col = new_col;
		if(glfwGetMouseButton(GLFW_MOUSE_BUTTON_1) == GLFW_PRESS) {
			touch();
		}
	}
	if(mouse_x != x || mouse_y != y) {
		if(glfwGetMouseButton(GLFW_MOUSE_BUTTON_2) == GLFW_PRESS || glfwGetMouseButton(GLFW_MOUSE_BUTTON_3) == GLFW_PRESS) {
			x_translate += ((x - mouse_x) / (float) cell_size) * CELL;
			y_translate += ((y - mouse_y) / (float) cell_size) * CELL;
			translate = glm::translate(glm::mat4(1.0f), glm::vec3(x_translate, y_translate, 0.0f));

			glUseProgram(program);
			glUniformMatrix4fv(translate_location, 1, GL_FALSE, &translate[0][0]);
			glUseProgram(0);
		}
		mouse_x = x;
		mouse_y = y;
	}
}

void GLFWCALL wheel_handler(int pos) {
	if(pos == 1) { cell_size += 1; }
	else if(pos == -1) { cell_size -= cell_size > 1 ? 1 : 0; }

	scale = glm::scale(glm::mat4(1.0f), glm::vec3((float) cell_size / CELL, (float) cell_size / CELL, 1.0f));
	glUseProgram(program);
	glUniformMatrix4fv(scale_location, 1, GL_FALSE, &scale[0][0]);
	glUseProgram(0);

	glfwSetMouseWheel(0);
}

/*
 * Print interoperable OpenCL device info
 */
void printDeviceInfo(cl_device_id device) {
	// Read device name
	size_t name_length;
	clGetDeviceInfo(device, CL_DEVICE_NAME, 0, NULL, &name_length);
	char *name = new char[name_length];
	clGetDeviceInfo(device, CL_DEVICE_NAME, name_length, name, NULL);

	std::cout << "=-- Selected interop device:" << device << "\t" << name << std::endl;

	delete name;
}

/*
 * Initialize Window and OpenGL context
 */
void initDisplay(unsigned int width, unsigned int height) {
	glfwInit();

	glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
	glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 3);
	glfwOpenWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwOpenWindowHint(GLFW_FSAA_SAMPLES, 4);
	if(!glfwOpenWindow(width, height, 0, 0, 0, 0, 32, 0, GLFW_WINDOW)) {
		std::cerr << "GLFW: unable to open window" << std::endl;
		running = 0;
	} else {
		if (glewInit() != GLEW_OK) {
			std::cerr << "GLEW: failed to initialize" << std::endl;
			running = 0;
		}
		glfwSetWindowTitle("CL rock, paper, scissors");
		glfwSetWindowCloseCallback(close_handler);
		glfwSetWindowSizeCallback(window_size);
		glfwSetKeyCallback(key_handler);
		glfwSetCharCallback(char_handler);
		glfwSetMouseButtonCallback(button_handler);
		glfwSetMousePosCallback(pos_handler);
		glfwSetMouseWheelCallback(wheel_handler);
		glfwSwapBuffers();
		// Reset GL error
		glGetError();
	}
}

void exitDisplay() {
	std::cout << "= Display cleanup" << std::endl;
	glfwTerminate();
}

/*
 * Initialize OpenGL
 */
void initGL() {
	std::cout << "= GL inizialization" << std::endl;
	glClearColor(0.1, 0.1, 0.4, 0.0);

	// Load shader program
	program = loadShader("vertex_shader.glsl", "fragment_shader.glsl");

	// Get texture uniform location
	universe_texture_location  		= glGetUniformLocation(program, "universe");
	background_texture_location  	= glGetUniformLocation(program, "background");
	view_location  					= glGetUniformLocation(program, "view");
	scale_location  				= glGetUniformLocation(program, "scale");
	perspective_location  			= glGetUniformLocation(program, "perspective");
	translate_location				= glGetUniformLocation(program, "translate");
	frame_location					= glGetUniformLocation(program, "frame");

	// Create vao for fullscreen quad
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	GLuint uvbo;
	glGenBuffers(1, &uvbo);
	glBindBuffer(GL_ARRAY_BUFFER, uvbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_uv_buffer_data), g_uv_buffer_data, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	GLuint ibo;
	glGenBuffers(1, &ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(g_index_buffer_data), g_index_buffer_data, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glVertexAttribPointer(
			0,
			4,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0
	);
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, uvbo);
	glVertexAttribPointer(
			1,
			2,
			GL_FLOAT,
			GL_FALSE,
			0,
			(void*)0
	);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
	glBindVertexArray(0);

	// Generate textures
	glGenTextures(1, &universe_texture);
	glBindTexture(GL_TEXTURE_2D, universe_texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, WIDTH, HEIGHT, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBindTexture(GL_TEXTURE_2D, 0);

	glGenTextures(1, &update_texture);
	glBindTexture(GL_TEXTURE_2D, update_texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, WIDTH, HEIGHT, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBindTexture(GL_TEXTURE_2D, 0);

	GLubyte *bg = new GLubyte[WIDTH * HEIGHT];
	for(int i = 0; i < WIDTH; i++) {
		for(int j = 0; j < HEIGHT; j++) {
			bg[i * WIDTH + j] = (i + j) % 2 * 0x11;
		}
	}

	glGenTextures(1, &background_texture);
	glBindTexture(GL_TEXTURE_2D, background_texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, WIDTH, HEIGHT, 0, GL_RED, GL_UNSIGNED_BYTE, bg);
	delete bg;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBindTexture(GL_TEXTURE_2D, 0);

	view = glm::lookAt(glm::vec3(0, 0, 1), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
	perspective = glm::ortho(0.0f, 512.0f, 0.0f, 512.0f);
	translate = glm::translate(glm::mat4(1.0f), glm::vec3(x_translate, y_translate, 0.0f));
	scale = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, 1.0f));

	glUseProgram(program);
	glUniformMatrix4fv(perspective_location, 1, GL_FALSE, &perspective[0][0]);
	glUniformMatrix4fv(view_location, 1, GL_FALSE, &view[0][0]);
	glUniformMatrix4fv(translate_location, 1, GL_FALSE, &translate[0][0]);
	glUniformMatrix4fv(scale_location, 1, GL_FALSE, &scale[0][0]);

	glUniform1i(universe_texture_location, 0);
	glUniform1i(background_texture_location, 1);
	glUseProgram(0);
}

void exitGL() {
	std::cout << "= GL cleanup" << std::endl;

	glDeleteVertexArrays(1, &vao);
	glDeleteTextures(1, &universe_texture);
	glDeleteTextures(1, &update_texture);
	glDeleteTextures(1, &background_texture);
	glDeleteProgram(program);
}

/*
 * Initialize OpenCL
 */
void initCL() {
	std::cout << "= CL initialization" << std::endl;
	cl_int clError;

	// Query the current platform
	clGetPlatformIDs(1, &platform, NULL);

	// Query extension function handler
	clGetGLContextInfoKHR_fn clGetGLContextInfoKHR = (clGetGLContextInfoKHR_fn) clGetExtensionFunctionAddressForPlatform(platform, "clGetGLContextInfoKHR");

	// Set up context properties
#if defined _WIN32
	cl_context_properties properties[] = {
			CL_GL_CONTEXT_KHR,		(cl_context_properties) wglGetCurrentContext(),
			CL_WGL_HDC_KHR,			(cl_context_properties) wglGetCurrentDC(),
			CL_CONTEXT_PLATFORM,	(cl_context_properties) platform,
			0
	};
#elif defined __linux__
	cl_context_properties properties[] = {
			CL_GL_CONTEXT_KHR,		(cl_context_properties) glXGetCurrentContext(),
			CL_GLX_DISPLAY_KHR,		(cl_context_properties) glXGetCurrentDisplay(),
			CL_CONTEXT_PLATFORM,	(cl_context_properties) platform,
			0
	};
#endif

	// Get the current device capable for CL-GL interop
	clGetGLContextInfoKHR(properties, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof(cl_device_id), &device, NULL);

	// Print selected device info
	printDeviceInfo(device);

	// Create context from GL context
	context = clCreateContext(properties, 1, &device, NULL, NULL, &clError);

	// Create command queue for device
	queue = clCreateCommandQueue(context, device, 0, NULL);

	std::stringstream build_options;
	build_options << "-D WIDTH=" << WIDTH << " -D HEIGHT=" << HEIGHT;
	kernel = loadKernel(context, device, "clrps_kernel.cl", build_options.str().data(), "rps");

	// Create image buffers from GL textures
	universe_buffer = clCreateFromGLTexture(context, CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0, universe_texture, 	&clError);
	update_buffer 	= clCreateFromGLTexture(context, CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0, update_texture, 	&clError);

	random_buffer	= clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(GLfloat) * WIDTH * HEIGHT, NULL, &clError);

	// Sampler for kernel
	sampler = clCreateSampler(context, CL_TRUE, CL_ADDRESS_REPEAT, CL_FILTER_NEAREST, &clError);
}

void exitCL() {
	std::cout << "= CL cleanup" << std::endl;
	clReleaseMemObject(universe_buffer);
	clReleaseMemObject(update_buffer);
	clReleaseMemObject(random_buffer);
	clReleaseSampler(sampler);
	clReleaseKernel(kernel);
	clReleaseCommandQueue(queue);
	clReleaseContext(context);
}

int main() {
	signal(SIGINT, exit_handler);

	initDisplay(window_width, window_height);

	initGL();
	initCL();

	randomize();

	std::cout << std::endl << "= Running." << std::endl;
	unsigned int frame = 0;
	const double loop_time = 1.0 / FPS;
    while(running) {
    	double start_time = glfwGetTime();
    	// Render
    	glClear(GL_COLOR_BUFFER_BIT);

    	glUseProgram(program);
    	glBindVertexArray(vao);

    	glActiveTexture(GL_TEXTURE0);
    	glBindTexture(GL_TEXTURE_2D, universe_texture);

    	glUniform1f(frame_location, (float) frame);

    	glActiveTexture(GL_TEXTURE1);
    	glBindTexture(GL_TEXTURE_2D, background_texture);

    	glDrawElements(
    			GL_TRIANGLES,
    			6,
    			GL_UNSIGNED_INT,
    			(void*)0
    	);
    	glBindVertexArray(0);
    	glUseProgram(0);

    	glfwSwapBuffers();

    	frame++;

    	// Update
    	if(!pause) {
    		clEnqueueAcquireGLObjects(queue, 2, buffers, 0, NULL, NULL);

    		clSetKernelArg(kernel, 0, sizeof(cl_mem), &universe_buffer);
    		clSetKernelArg(kernel, 1, sizeof(cl_mem), &update_buffer);
    		clSetKernelArg(kernel, 2, sizeof(cl_mem), &random_buffer);
    		clSetKernelArg(kernel, 3, sizeof(cl_sampler), &sampler);

    		const size_t work_size[] = {WIDTH, HEIGHT};
    		if(clEnqueueNDRangeKernel(queue, kernel, 2, NULL, work_size, NULL, 0, NULL, NULL)) {std::cerr << "Kernel runtime error!" << std::endl;}

    		cl_mem temp_mem = universe_buffer;
    		universe_buffer = update_buffer;
    		update_buffer = temp_mem;

    		GLuint temp_texture;
    		temp_texture = universe_texture;
    		universe_texture = update_texture;
    		update_texture = temp_texture;

    		clEnqueueReleaseGLObjects(queue, 2, buffers, 0, NULL, NULL);
    		clFinish(queue);
    	}

    	// Frame rate control
    	double work_time = glfwGetTime() - start_time;
    	double sleep_time = loop_time - work_time;
		glfwSleep(sleep_time);
    }

	exitCL();
	exitGL();
	exitDisplay();

	exit(0);
}
