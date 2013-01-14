#include "utils.hpp"
#include <iostream>
#include <fstream>
#include <ostream>
#include <string.h>

using namespace std;

std::string readFile(const char *file_path) {
	ifstream file_stream(file_path);

	string str((istreambuf_iterator<char>(file_stream)), istreambuf_iterator<char>());
	file_stream.close();

	return str;
}

GLuint loadShader(const char *vertex_file_path, const char *fragment_file_path) {
	GLint error;

	std::string vertex_shader_source 		= readFile(vertex_file_path);
	std::string fragment_shader_source 	= readFile(fragment_file_path);

	const char *vertex_shader_source_ptr = vertex_shader_source.c_str();

	GLuint vertex_shader 	= glCreateShader(GL_VERTEX_SHADER);
	GLuint fragment_shader 	= glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(vertex_shader, 1, &vertex_shader_source_ptr , NULL);
	glCompileShader(vertex_shader);

	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &error);
	if(!error) {
		std::cerr << "Unable to compile vertex shader: " << std::endl;
		GLint log_length;
		glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &log_length);
		char *info_log = new char[log_length];
		glGetShaderInfoLog(vertex_shader, log_length, NULL, info_log);
		std::cerr << info_log;
		delete info_log;
		return 0;
	}

	const char *fragment_shader_source_ptr = fragment_shader_source.c_str();

	glShaderSource(fragment_shader, 1, &fragment_shader_source_ptr , NULL);
	glCompileShader(fragment_shader);

	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &error);
	if(!error) {
		std::cerr << "Unable to compile fragment shader: " << std::endl;
		GLint log_length;
		glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &log_length);
		char *info_log = new char[log_length];
		glGetShaderInfoLog(fragment_shader, log_length, NULL, info_log);
		std::cerr << info_log;
		delete info_log;
		return 0;
	}

	GLuint program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &error);
	if(!error) {
		std::cerr << "Unable to link shader program: " << std::endl;
		GLint log_length;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
		char *info_log = new char[log_length];
		glGetProgramInfoLog(program, log_length, NULL, info_log);
		std::cerr << info_log;
		delete info_log;
		return 0;
	}

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	return program;
}

cl_kernel loadKernel(const cl_context context, const cl_device_id device, const char *kernel_file, const char* build_options, const char* kernel_name) {
	cl_int error;
	std::string source = readFile(kernel_file);

	const char *source_ptr = source.data();
	const unsigned int length = source.size();

	cl_program program = clCreateProgramWithSource(context, 1, (const char**) &source_ptr, &length, &error);
	if(error) {
		std::cerr << "Unable to create program: " << error << std::endl;
		return NULL;
	}

	error = clBuildProgram(program, 1, &device, build_options, NULL, NULL);
	if(error) {
		char *build_log;
		unsigned int log_length;
		std::cerr << "Error building program: " << error << std::endl;
		clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_length);
		build_log = new char[log_length];
		clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_length, build_log, NULL);
		std::cerr << build_log;
		delete build_log;

		clReleaseProgram(program);

		return NULL;
	}

	cl_kernel kernel = clCreateKernel(program, kernel_name, &error);
	if(error) {
		std::cerr << "Unable to create kernel: " << error << std::endl;
		return NULL;
	}
	clReleaseProgram(program);

	return kernel;
}
