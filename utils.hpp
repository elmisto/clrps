#define __NO_STD_VECTOR // Use cl::vector instead of STL version
#include <CL/cl.h>

#include <GL/glew.h>

#define GLFW_INCLUDE_GL3
#include "GL/glfw.h"
#include <string>
#include <vector>

#ifndef UTILS_HPP
#define UTILS_HPP

std::string loadFile(const char *filePath);

/*
 * Platform --.---------.----------------.
 *            |         |                |
 *            ¡         ¡                ¡
 *            Device    Device    ...    Device ----.
 *                     [_______________________]    |
 *                                 |                |--> Queue
 *                                 ¡                |
 *                              Context ------------'
 *
 */

const char *getDeviceName(cl_device_id device);

cl_device_id *getDevices(cl_device_type type);

std::string readFile(const char *file_path);

GLuint loadShader(const char *vertexFile, const char *fragmentFile);
cl_kernel loadKernel(const cl_context context, const cl_device_id device, const char *kernel_file, const char* build_options, const char* kernel_name);

#endif //UTILS_HPP
