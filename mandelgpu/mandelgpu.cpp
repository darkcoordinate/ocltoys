/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of OCLToys.                                         *
 *                                                                         *
 *   OCLToys is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   OCLToys is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   OCLToys website: http://code.google.com/p/ocltoys                     *
 ***************************************************************************/

#include "ocltoy.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <fstream>

#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>

// Options
static int useCPU = 0;
static int useGPU = 1;

// OpenCL variables
static cl_context context;
static cl_device_id *devices = NULL;
static cl_mem pixelBuffer;
static cl_command_queue commandQueue;
static cl_program program;
static cl_kernel kernel;
static char *kernelFileName = "rendering_kernel_float4.cl";

unsigned int workGroupSize = 1;

static char *ReadSources(const char *fileName) {
	FILE *file = fopen(fileName, "r");
	if (!file) {
		fprintf(stderr, "Failed to open file '%s'\n", fileName);
		exit(-1);
	}

	if (fseek(file, 0, SEEK_END)) {
		fprintf(stderr, "Failed to seek file '%s'\n", fileName);
		exit(-1);
	}

	long size = ftell(file);
	if (size == 0) {
		fprintf(stderr, "Failed to check position on file '%s'\n", fileName);
		exit(-1);
	}

	rewind(file);

	char *src = (char *)malloc(sizeof(char) * size + 1);
	if (!src) {
		fprintf(stderr, "Failed to allocate memory for file '%s'\n", fileName);
		exit(-1);
	}

	fprintf(stderr, "Reading file '%s' (size %ld bytes)\n", fileName, size);
	size_t res = fread(src, 1, sizeof(char) * size, file);
	if (res != sizeof(char) * size) {
		fprintf(stderr, "Failed to read file '%s' (read %ld)\n", fileName, res);
		exit(-1);
	}
	src[size] = '\0'; // NULL terminated

	fclose(file);

	return src;

}

class MandelGPU : public OCLToy {
public:
	MandelGPU() : OCLToy("MandelGPU v" OCLTOYS_VERSION_MAJOR "." OCLTOYS_VERSION_MINOR " (OCLToys: http://code.google.com/p/ocltoys)"),
			scale(3.5f), offsetX(-.5f), offsetY(0.f), maxIterations(256), pixels(NULL),
			mouseButton0(0), mouseButton2(0), mouseGrabLastX(0), mouseGrabLastY(0) {
	}
	virtual ~MandelGPU() { }

protected:
	boost::program_options::options_description GetOptionsDescriction() {
		boost::program_options::options_description opts("MandelGPU options");

		opts.add_options()
			("kernel,k", boost::program_options::value<std::string>(), "Kernel file name");

		return opts;
	}

	virtual int RunToy() {
		InitGlut();

		SetUpOpenCL();
		UpdateMandel();

		glutMainLoop();

		return EXIT_SUCCESS;
	}
	
	virtual void DisplayCallBack() {
		UpdateMandel();

		glClear(GL_COLOR_BUFFER_BIT);
		glRasterPos2i(0, 0);
		glDrawPixels(windowWidth, windowHeight, GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels);

		// Title
		glColor3f(1.f, 1.f, 1.f);
		glRasterPos2i(4, windowHeight - 16);
		PrintString(GLUT_BITMAP_HELVETICA_18, windowTitle);

		// Caption line 0
		glColor3f(1.f, 1.f, 1.f);
		glRasterPos2i(4, 10);
		PrintString(GLUT_BITMAP_HELVETICA_18, captionBuffer);

		if (printHelp) {
			glPushMatrix();
			glLoadIdentity();
			glOrtho(0.f, 640.f, 0.f, 480.f, -1.0, 1.0);

			PrintHelp();

			glPopMatrix();
		}

		glutSwapBuffers();
	}

	virtual void ReshapeCallBack(int newWidth, int newHeight) {
		// Width must be a multiple of 4
		windowWidth = newWidth;
		if (windowWidth % 4 != 0)
			windowWidth = (windowWidth / 4 + 1) * 4;
		windowHeight = newHeight;

		glViewport(0, 0, windowWidth, windowHeight);
		glLoadIdentity();
		glOrtho(0.f, windowWidth - 1.f,
				0.f, windowHeight - 1.f, -1.f, 1.f);

		FreeBuffers();
		AllocateBuffers();

		glutPostRedisplay();
	}

	virtual void KeyCallBack(unsigned char key, int x, int y) {
		bool needRedisplay = true;

		switch (key) {
			case 's': {
				// Write image to PPM file
				std::ofstream f("image.ppm");
				if (!f.good()) {
					OCLTOY_LOG("Failed to open image file: image.ppm");
				} else {
					f << "P3" << std::endl;
					f << windowWidth << " " << windowHeight << std::endl;
					f << "255" << std::endl;
					
					int x, y;
					for (y = windowHeight - 1; y >= 0; --y) {
						const unsigned char *p = (unsigned char *)(&pixels[y * windowWidth / 4]);
						for (x = 0; x < windowWidth; ++x, p++) {
							f << *p << " " << *p << " " << *p << std::endl;
						}
					}
				}				
				f.close();

				needRedisplay = false;
				break;
			}
			case 27: // Escape key
			case 'q':
			case 'Q':
				OCLTOY_LOG("Done");
				exit(EXIT_SUCCESS);
				break;
			case '+':
				maxIterations += 32;
				break;
			case '-':
				maxIterations -= 32;
				break;
			case ' ': // Refresh display
				break;
			case 'h':
				printHelp = (!printHelp);
				break;
			default:
				needRedisplay = false;
				break;
		}

		if (needRedisplay) {
			UpdateMandel();
			glutPostRedisplay();
		}
	}

	void SpecialCallBack(int key, int x, int y) {
#define SCALE_STEP (0.1f)
#define OFFSET_STEP (0.025f)
		bool needRedisplay = true;

		switch (key) {
			case GLUT_KEY_UP:
				offsetY += scale * OFFSET_STEP;
				break;
			case GLUT_KEY_DOWN:
				offsetY -= scale * OFFSET_STEP;
				break;
			case GLUT_KEY_LEFT:
				offsetX -= scale * OFFSET_STEP;
				break;
			case GLUT_KEY_RIGHT:
				offsetX += scale * OFFSET_STEP;
				break;
			case GLUT_KEY_PAGE_UP:
				scale *= 1.f - SCALE_STEP;
				break;
			case GLUT_KEY_PAGE_DOWN:
				scale *= 1.f + SCALE_STEP;
				break;
			default:
				needRedisplay = false;
				break;
		}

		if (needRedisplay) {
			UpdateMandel();
			glutPostRedisplay();
		}
	}

	void MouseCallBack(int button, int state, int x, int y) {
		if (button == 0) {
			if (state == GLUT_DOWN) {
				// Record start position
				mouseGrabLastX = x;
				mouseGrabLastY = y;
				mouseButton0 = 1;
			} else if (state == GLUT_UP) {
				mouseButton0 = 0;
			}
		} else if (button == 2) {
			if (state == GLUT_DOWN) {
				// Record start position
				mouseGrabLastX = x;
				mouseGrabLastY = y;
				mouseButton2 = 1;
			} else if (state == GLUT_UP) {
				mouseButton2 = 0;
			}
		}
	}

	virtual void MotionCallBack(int x, int y) {
		int needRedisplay = 1;

		if (mouseButton0) {
			const int distX = x - mouseGrabLastX;
			const int distY = y - mouseGrabLastY;

			offsetX -= (40.f * distX / windowWidth) * scale * OFFSET_STEP;
			offsetY += (40.f * distY / windowHeight) * scale * OFFSET_STEP;

			mouseGrabLastX = x;
			mouseGrabLastY = y;
		} else if (mouseButton2) {
			const int distX = x - mouseGrabLastX;

			scale *= 1.0f - (2.f * distX / windowWidth);

			mouseGrabLastX = x;
			mouseGrabLastY = y;
		} else
			needRedisplay = 0;

		if (needRedisplay)
			glutPostRedisplay();
	}

private:
	void FreeBuffers() {
		const cl_int status = clReleaseMemObject(pixelBuffer);
		if (status != CL_SUCCESS) {
			fprintf(stderr, "Failed to release OpenCL output buffer: %d\n", status);
			exit(-1);
		}

		free(pixels);
	}

	void AllocateBuffers() {
		const int pixelCount = windowWidth * windowHeight;
		// Szaq's patch for NVIDIA OpenCL and Windows
		cl_uint sizeBytes = sizeof(int) * (pixelCount / 4 + 1);
		pixels = (unsigned int *)malloc(sizeBytes);
		memset(pixels, 255, sizeBytes);

		cl_int status;
		pixelBuffer = clCreateBuffer(
				context,
				CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR,
				sizeBytes,
				pixels,
				&status);
		if (status != CL_SUCCESS) {
			fprintf(stderr, "Failed to create OpenCL output buffer: %d\n", status);
			exit(-1);
		}
	}

	void SetUpOpenCL() {
		cl_device_type dType;
		if (useCPU) {
			if (useGPU)
				dType = CL_DEVICE_TYPE_ALL;
			else
				dType = CL_DEVICE_TYPE_CPU;
		} else {
			if (useGPU)
				dType = CL_DEVICE_TYPE_GPU;
			else
				dType = CL_DEVICE_TYPE_DEFAULT;
		}

		cl_uint numPlatforms;
		cl_platform_id platform = NULL;
		cl_int status = clGetPlatformIDs(0, NULL, &numPlatforms);
		if (status != CL_SUCCESS) {
			fprintf(stderr, "Failed to get OpenCL platforms\n");
			exit(-1);
		}

		if (numPlatforms > 0) {
			cl_platform_id *platforms = (cl_platform_id *)malloc(sizeof(cl_platform_id) * numPlatforms);
			status = clGetPlatformIDs(numPlatforms, platforms, NULL);
				if (status != CL_SUCCESS) {
				fprintf(stderr, "Failed to get OpenCL platform IDs\n");
				exit(-1);
			}

			unsigned int i;
			for (i = 0; i < numPlatforms; ++i) {
				char pbuf[100];
				status = clGetPlatformInfo(platforms[i],
						CL_PLATFORM_VENDOR,
						sizeof(pbuf),
						pbuf,
						NULL);

				status = clGetPlatformIDs(numPlatforms, platforms, NULL);
				if (status != CL_SUCCESS) {
					fprintf(stderr, "Failed to get OpenCL platform IDs\n");
					exit(-1);
				}

				fprintf(stderr, "OpenCL Platform %d: %s\n", i, pbuf);
			}

			platform = platforms[0];
			free(platforms);
		}

		cl_context_properties cps[3] ={
			CL_CONTEXT_PLATFORM,
			(cl_context_properties) platform,
			0
		};

		cl_context_properties *cprops = (NULL == platform) ? NULL : cps;

		context = clCreateContextFromType(
				cprops,
				dType,
				NULL,
				NULL,
				&status);
		if (status != CL_SUCCESS) {
			fprintf(stderr, "Failed to open OpenCL context\n");
			exit(-1);
		}

		// Get the size of device list data
		size_t deviceListSize;
		status = clGetContextInfo(
				context,
				CL_CONTEXT_DEVICES,
				0,
				NULL,
				&deviceListSize);
		if (status != CL_SUCCESS) {
			fprintf(stderr, "Failed to get OpenCL context info size: %d\n", status);
			exit(-1);
		}

		devices = (cl_device_id *) malloc(deviceListSize);
		if (devices == NULL) {
			fprintf(stderr, "Failed to allocate memory for OpenCL device list: %d\n", status);
			exit(-1);
		}

		// Get the device list data
		status = clGetContextInfo(
				context,
				CL_CONTEXT_DEVICES,
				deviceListSize,
				devices,
				NULL);
		if (status != CL_SUCCESS) {
			fprintf(stderr, "Failed to get OpenCL context info: %d\n", status);
			exit(-1);
		}

		// Print devices list
		unsigned int i;
		for (i = 0; i < deviceListSize / sizeof(cl_device_id); ++i) {
			cl_device_type type = 0;
			status = clGetDeviceInfo(devices[i],
					CL_DEVICE_TYPE,
					sizeof(cl_device_type),
					&type,
					NULL);
			if (status != CL_SUCCESS) {
				fprintf(stderr, "Failed to get OpenCL device info: %d\n", status);
				exit(-1);
			}

			char *stype;
			switch (type) {
				case CL_DEVICE_TYPE_ALL:
					stype = "TYPE_ALL";
					break;
				case CL_DEVICE_TYPE_DEFAULT:
					stype = "TYPE_DEFAULT";
					break;
				case CL_DEVICE_TYPE_CPU:
					stype = "TYPE_CPU";
					break;
				case CL_DEVICE_TYPE_GPU:
					stype = "TYPE_GPU";
					break;
				default:
					stype = "TYPE_UNKNOWN";
					break;
			}
			fprintf(stderr, "OpenCL Device %d: Type = %s\n", i, stype);

			char buf[256];
			status = clGetDeviceInfo(devices[i],
					CL_DEVICE_NAME,
					sizeof(char[256]),
					&buf,
					NULL);
			if (status != CL_SUCCESS) {
				fprintf(stderr, "Failed to get OpenCL device info: %d\n", status);
				exit(-1);
			}

			fprintf(stderr, "OpenCL Device %d: Name = %s\n", i, buf);

			cl_uint units = 0;
			status = clGetDeviceInfo(devices[i],
					CL_DEVICE_MAX_COMPUTE_UNITS,
					sizeof(cl_uint),
					&units,
					NULL);
			if (status != CL_SUCCESS) {
				fprintf(stderr, "Failed to get OpenCL device info: %d\n", status);
				exit(-1);
			}

			fprintf(stderr, "OpenCL Device %d: Compute units = %u\n", i, units);

			size_t gsize = 0;
			status = clGetDeviceInfo(devices[i],
					CL_DEVICE_MAX_WORK_GROUP_SIZE,
					sizeof (size_t),
					&gsize,
					NULL);
			if (status != CL_SUCCESS) {
				fprintf(stderr, "Failed to get OpenCL device info: %d\n", status);
				exit(-1);
			}

			fprintf(stderr, "OpenCL Device %d: Max. work group size = %u\n", i, (unsigned int)gsize);
		}

		AllocateBuffers();

		// Create the kernel program
		const char *sources = ReadSources(kernelFileName);
		program = clCreateProgramWithSource(
				context,
				1,
				&sources,
				NULL,
				&status);
		if (status != CL_SUCCESS) {
			fprintf(stderr, "Failed to open OpenCL kernel sources: %d\n", status);
			exit(-1);
		}

		status = clBuildProgram(program, 1, devices, NULL, NULL, NULL);
		if (status != CL_SUCCESS) {
			fprintf(stderr, "Failed to build OpenCL kernel: %d\n", status);

			size_t retValSize;
			status = clGetProgramBuildInfo(
					program,
					devices[0],
					CL_PROGRAM_BUILD_LOG,
					0,
					NULL,
					&retValSize);
			if (status != CL_SUCCESS) {
				fprintf(stderr, "Failed to get OpenCL kernel info size: %d\n", status);
				exit(-1);
			}

			// Szaq's patch for NVIDIA OpenCL and Windows
			char *buildLog = (char *)malloc(retValSize + 1);
			status = clGetProgramBuildInfo(
					program,
					devices[0],
					CL_PROGRAM_BUILD_LOG,
					retValSize,
					buildLog,
					NULL);
			if (status != CL_SUCCESS) {
				fprintf(stderr, "Failed to get OpenCL kernel info: %d\n", status);
				exit(-1);
			}
			buildLog[retValSize] = '\0';

			fprintf(stderr, "OpenCL Programm Build Log: %s\n", buildLog);
			exit(-1);
		}

		kernel = clCreateKernel(program, "mandelGPU", &status);
		if (status != CL_SUCCESS) {
			fprintf(stderr, "Failed to create OpenCL kernel: %d\n", status);
			exit(-1);
		}

		// LordCRC's patch for better workGroupSize
		size_t gsize = 0;
		status = clGetKernelWorkGroupInfo(kernel,
				devices[0],
				CL_KERNEL_WORK_GROUP_SIZE,
				sizeof (size_t),
				&gsize,
				NULL);
		if (status != CL_SUCCESS) {
			fprintf(stderr, "Failed to get OpenCL kernel work group size info: %d\n", status);
			exit(-1);
		}

		workGroupSize = (unsigned int) gsize;
		fprintf(stderr, "OpenCL Device 0: kernel work group size = %d\n", workGroupSize);

		cl_command_queue_properties prop = 0;
		commandQueue = clCreateCommandQueue(
				context,
				devices[0],
				prop,
				&status);
		if (status != CL_SUCCESS) {
			fprintf(stderr, "Failed to create OpenCL command queue: %d\n", status);
			exit(-1);
		}
	}

	void UpdateMandel() {
		const double startTime = WallClockTime();

		// Set kernel arguments
		cl_int status = clSetKernelArg(
				kernel,
				0,
				sizeof(cl_mem),
				(void *) &pixelBuffer);
		if (status != CL_SUCCESS) {
			fprintf(stderr, "Failed to set OpenCL arg. #1: %d\n", status);
			exit(-1);
		}

		status = clSetKernelArg(
				kernel,
				1,
				sizeof(int),
				(void *)&windowWidth);
		if (status != CL_SUCCESS) {
			fprintf(stderr, "Failed to set OpenCL arg. #2: %d\n", status);
			exit(-1);
		}

		status = clSetKernelArg(
				kernel,
				2,
				sizeof(int),
				(void *)&windowHeight);
		if (status != CL_SUCCESS) {
			fprintf(stderr, "Failed to set OpenCL arg. #2: %d\n", status);
			exit(-1);
		}

		status = clSetKernelArg(
				kernel,
				3,
				sizeof(float),
				(void *)&scale);
		if (status != CL_SUCCESS) {
			fprintf(stderr, "Failed to set OpenCL arg. #2: %d\n", status);
			exit(-1);
		}

		status = clSetKernelArg(
				kernel,
				4,
				sizeof(float),
				(void *)&offsetX);
		if (status != CL_SUCCESS) {
			fprintf(stderr, "Failed to set OpenCL arg. #2: %d\n", status);
			exit(-1);
		}

		status = clSetKernelArg(
				kernel,
				5,
				sizeof(float),
				(void *)&offsetY);
		if (status != CL_SUCCESS) {
			fprintf(stderr, "Failed to set OpenCL arg. #2: %d\n", status);
			exit(-1);
		}

		status = clSetKernelArg(
				kernel,
				6,
				sizeof(int),
				(void *)&maxIterations);
		if (status != CL_SUCCESS) {
			fprintf(stderr, "Failed to set OpenCL arg. #2: %d\n", status);
			exit(-1);
		}

		// Enqueue a kernel run call
		cl_event events[2];
		size_t globalThreads[1];
		globalThreads[0] = windowWidth * windowHeight / 4 + 1;
		if (globalThreads[0] % workGroupSize != 0)
				globalThreads[0] = (globalThreads[0] / workGroupSize + 1) * workGroupSize;
		size_t localThreads[1];
		localThreads[0] = workGroupSize;

		status = clEnqueueNDRangeKernel(
				commandQueue,
				kernel,
				1,
				NULL,
				globalThreads,
				localThreads,
				0,
				NULL,
				&events[0]);
		if (status != CL_SUCCESS) {
			fprintf(stderr, "Failed to enqueue OpenCL work: %d\n", status);
			exit(-1);
		}

		// Wait for the kernel call to finish execution
		status = clWaitForEvents(1, &events[0]);
		if (status != CL_SUCCESS) {
			fprintf(stderr, "Failed to wait the end of OpenCL execution: %d\n", status);
			exit(-1);
		}
		clReleaseEvent(events[0]);

		// Enqueue readBuffer
		status = clEnqueueReadBuffer(
				commandQueue,
				pixelBuffer,
				CL_TRUE,
				0,
				windowWidth * windowHeight,
				pixels,
				0,
				NULL,
				&events[1]);
		if (status != CL_SUCCESS) {
			fprintf(stderr, "Failed to the OpenCL output buffer: %d\n", status);
			exit(-1);
		}
		clReleaseEvent(events[1]);

		const double elapsedTime = WallClockTime() - startTime;
		const double sampleSec = windowHeight * windowWidth / elapsedTime;
		sprintf(captionBuffer, "Rendering time: %.3f secs (Sample/sec %.1fK Max. Iterations %d)",
				elapsedTime, sampleSec / 1000.f, maxIterations);
	}

	void PrintHelp() {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(0.f, 0.f, 0.5f, 0.5f);
		glRecti(40, 40, 600, 440);

		glColor3f(1.f, 1.f, 1.f);
		glRasterPos2i(300, 420);
		PrintString(GLUT_BITMAP_HELVETICA_18, "Help");

		glRasterPos2i(60, 390);
		PrintString(GLUT_BITMAP_HELVETICA_18, "h - toggle Help");
		glRasterPos2i(60, 360);
		PrintString(GLUT_BITMAP_HELVETICA_18, "arrow Keys - move left/right/up/down");
		glRasterPos2i(60, 330);
		PrintString(GLUT_BITMAP_HELVETICA_18, "PageUp and PageDown - zoom in/out");
		glRasterPos2i(60, 300);
		PrintString(GLUT_BITMAP_HELVETICA_18, "Mouse button 0 + Mouse X, Y - move left/right/up/down");
		glRasterPos2i(60, 270);
		PrintString(GLUT_BITMAP_HELVETICA_18, "Mouse button 2 + Mouse X - zoom in/out");
		glRasterPos2i(60, 240);
		PrintString(GLUT_BITMAP_HELVETICA_18, "+ - increase the max. interations by 32");
		glRasterPos2i(60, 210);
		PrintString(GLUT_BITMAP_HELVETICA_18, "- - decrease the max. interations by 32");

		glDisable(GL_BLEND);
	}

	float scale, offsetX, offsetY;
	int maxIterations;
	char captionBuffer[256];

	unsigned int *pixels;

	int mouseButton0, mouseButton2, mouseGrabLastX, mouseGrabLastY;
};

int main(int argc, char **argv) {
	MandelGPU toy;
	return toy.Run(argc, argv);
}
