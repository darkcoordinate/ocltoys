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

#include <iostream>
#include <fstream>
#include <string>

#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>

class MandelGPU : public OCLToy {
public:
	MandelGPU() : OCLToy("MandelGPU v" OCLTOYS_VERSION_MAJOR "." OCLTOYS_VERSION_MINOR " (OCLToys: http://code.google.com/p/ocltoys)"),
			scale(3.5f), offsetX(-.5f), offsetY(0.f), maxIterations(256),
			mouseButton0(false), mouseButton2(false), mouseGrabLastX(0), mouseGrabLastY(0),
			pixels(NULL), pixelsBuff(NULL), workGroupSize(64) {
	}
	virtual ~MandelGPU() {
		FreeBuffers();
	}

protected:
	boost::program_options::options_description GetOptionsDescriction() {
		boost::program_options::options_description opts("MandelGPU options");

		opts.add_options()
			("kernel,k", boost::program_options::value<std::string>()->default_value("rendering_kernel_float4.cl"),
				"OpenCL kernel file name")
			("workgroupsize,z", boost::program_options::value<size_t>(), "OpenCL workgroup size");

		return opts;
	}

	virtual int RunToy() {
		SetUpOpenCL();
		UpdateMandel();

		glutMainLoop();

		return EXIT_SUCCESS;
	}

	//--------------------------------------------------------------------------
	// GLUT related code
	//--------------------------------------------------------------------------

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
		PrintString(GLUT_BITMAP_HELVETICA_18, captionString);

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

		AllocateBuffers();

		glutPostRedisplay();
	}

	virtual void KeyCallBack(unsigned char key, int x, int y) {
		bool needRedisplay = true;

		switch (key) {
			case 'p': {
				// Write image to PPM file
				std::ofstream f("image.ppm", std::ofstream::trunc);
				if (!f.good()) {
					OCLTOY_LOG("Failed to open image file: image.ppm");
				} else {
					f << "P3" << std::endl;
					f << windowWidth << " " << windowHeight << std::endl;
					f << "255" << std::endl;

					for (int y = windowHeight - 1; y >= 0; --y) {
						const unsigned char *p = (unsigned char *)(&pixels[y * windowWidth / 4]);
						for (int x = 0; x < windowWidth; ++x, p++) {
							const std::string value = boost::lexical_cast<std::string>((unsigned int)(*p));
							f << value << " " << value << " " << value << std::endl;
						}
					}
				}				
				f.close();
				OCLTOY_LOG("Saved framebuffer in image.ppm");

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
				mouseButton0 = true;
			} else if (state == GLUT_UP) {
				mouseButton0 = false;
			}
		} else if (button == 2) {
			if (state == GLUT_DOWN) {
				// Record start position
				mouseGrabLastX = x;
				mouseGrabLastY = y;
				mouseButton2 = true;
			} else if (state == GLUT_UP) {
				mouseButton2 = false;
			}
		}
	}

	virtual void MotionCallBack(int x, int y) {
		bool needRedisplay = true;

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
			needRedisplay = false;

		if (needRedisplay)
			glutPostRedisplay();
	}

	//--------------------------------------------------------------------------
	// OpenCL related code
	//--------------------------------------------------------------------------

	virtual unsigned int GetMaxDeviceCountSupported() const { return 1; }

private:
	void SetUpOpenCL() {
		//----------------------------------------------------------------------
		// Allocate buffer
		//----------------------------------------------------------------------

		AllocateBuffers();

		//----------------------------------------------------------------------
		// Compile kernel
		//----------------------------------------------------------------------

		const std::string &kernelFileName = commandLineOpts["kernel"].as<std::string>();
		OCLTOY_LOG("Compile OpenCL kernel: " << kernelFileName);

		// Read the kernel
		const std::string kernelSource = ReadSources(kernelFileName);

		// Create the kernel program
		cl::Device &oclDevice = selectedDevices[0];
		cl::Context &oclContext = deviceContexts[0];
		cl::Program::Sources source(1, std::make_pair(kernelSource.c_str(), kernelSource.length()));
		cl::Program program = cl::Program(oclContext, source);
		try {
			VECTOR_CLASS<cl::Device> buildDevice;
			buildDevice.push_back(oclDevice);
			program.build(buildDevice);
		} catch (cl::Error err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
			OCLTOY_LOG("Kernel compilation error:\n" << strError.c_str());

			throw err;
		}

		kernelMandel = cl::Kernel(program, "mandelGPU");
		kernelMandel.getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &workGroupSize);
		if (commandLineOpts.count("workgroupsize"))
			workGroupSize = commandLineOpts["workgroupsize"].as<size_t>();
		OCLTOY_LOG("Using workgroup size: " << workGroupSize);
	}

	void FreeBuffers() {
		FreeOCLBuffer(0, &pixelsBuff);
		delete[] pixels;
	}

	void AllocateBuffers() {
		delete[] pixels;
		const int pixelCount = windowWidth * windowHeight;
		const size_t size = pixelCount / 4 + 1;
		pixels = new unsigned int[size];
		std::fill(&pixels[0], &pixels[size], 0);

		AllocOCLBufferWO(0, &pixelsBuff, size * sizeof(unsigned int), "FrameBuffer");
	}

	void UpdateMandel() {
		const double startTime = WallClockTime();

		// Set kernel arguments
		kernelMandel.setArg(0, *pixelsBuff);
		kernelMandel.setArg(1, windowWidth);
		kernelMandel.setArg(2, windowHeight);
		kernelMandel.setArg(3, scale);
		kernelMandel.setArg(4, offsetX);
		kernelMandel.setArg(5, offsetY);
		kernelMandel.setArg(6, maxIterations);

		// Enqueue a kernel run
		size_t globalThreads = windowWidth * windowHeight / 4 + 1;
		if (globalThreads % workGroupSize != 0)
			globalThreads = (globalThreads / workGroupSize + 1) * workGroupSize;

		cl::CommandQueue &oclQueue = deviceQueues[0];
		oclQueue.enqueueNDRangeKernel(kernelMandel, cl::NullRange,
				cl::NDRange(globalThreads), cl::NDRange(workGroupSize));

		// Read back the result
		oclQueue.enqueueReadBuffer(
				*pixelsBuff,
				CL_TRUE,
				0,
				pixelsBuff->getInfo<CL_MEM_SIZE>(),
				pixels);

		const double elapsedTime = WallClockTime() - startTime;
		const double sampleSec = windowHeight * windowWidth / elapsedTime;
		captionString = boost::str(boost::format("Rendering time: %.3f secs (Sample/sec %.1fK Max. Iterations %d)") %
				elapsedTime % (sampleSec / 1000.0) % maxIterations);
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
		glRasterPos2i(60, 180);
		PrintString(GLUT_BITMAP_HELVETICA_18, "p - save image.ppm");

		glDisable(GL_BLEND);
	}

	float scale, offsetX, offsetY;
	int maxIterations;
	bool mouseButton0, mouseButton2;
	int mouseGrabLastX, mouseGrabLastY;

	unsigned int *pixels;
	cl::Buffer *pixelsBuff;

	cl::Kernel kernelMandel;
	size_t workGroupSize;

	std::string captionString;
};

int main(int argc, char **argv) {
	MandelGPU toy;
	return toy.Run(argc, argv);
}