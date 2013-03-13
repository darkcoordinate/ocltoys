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

#include "renderconfig.h"

class JuliaGPU : public OCLToy {
public:
	JuliaGPU() : OCLToy("JuliaGPU v" OCLTOYS_VERSION_MAJOR "." OCLTOYS_VERSION_MINOR " (OCLToys: http://code.google.com/p/ocltoys)"),
			mouseButton0(0), mouseButton2(0), mouseGrabLastX(0), mouseGrabLastY(0),
			pixels(NULL), pixelsBuff(NULL), configBuff(NULL), workGroupSize(64) {
		config.width = 640;
		config.height = 480;
		config.enableShadow = 1;
		config.superSamplingSize = 2;

		config.actvateFastRendering = 1;
		config.maxIterations = 9;
		config.epsilon = 0.003f * 0.75f;

		config.light[0] = 5.f;
		config.light[1] = 10.f;
		config.light[2] = 15.f;

		config.mu[0] = -0.2f;
		config.mu[1] = 0.4f;
		config.mu[2] = -0.4f;
		config.mu[3] = -0.4f;

		vinit(config.camera.orig, 1.f, 2.f, 8.f);
		vinit(config.camera.target, 0.f, 0.f, 0.f);

		UpdateCamera();
	}

	virtual ~JuliaGPU() {
		FreeBuffers();
	}

protected:
	boost::program_options::options_description GetOptionsDescriction() {
		boost::program_options::options_description opts("JuliaGPU options");

		opts.add_options()
			("kernel,k", boost::program_options::value<std::string>()->default_value("rendering_kernel.cl"),
				"OpenCL kernel file name")
			("workgroupsize,z", boost::program_options::value<size_t>(), "OpenCL workgroup size");

		return opts;
	}

	virtual int RunToy() {
		SetUpOpenCL();
		UpdateJulia();

		glutMainLoop();

		return EXIT_SUCCESS;
	}

	void UpdateCamera() {
		vsub(config.camera.dir, config.camera.target, config.camera.orig);
		vnorm(config.camera.dir);

		const Vec up = { 0.f, 1.f, 0.f };
		vxcross(config.camera.x, config.camera.dir, up);
		vnorm(config.camera.x);
		vsmul(config.camera.x, config.width * .5135f / config.height, config.camera.x);

		vxcross(config.camera.y, config.camera.x, config.camera.dir);
		vnorm(config.camera.y);
		vsmul(config.camera.y, .5135f, config.camera.y);
	}

	//--------------------------------------------------------------------------
	// GLUT related code
	//--------------------------------------------------------------------------

	virtual void DisplayCallBack() {
		UpdateJulia();

		glClear(GL_COLOR_BUFFER_BIT);
		glRasterPos2i(0, 0);
		glDrawPixels(config.width, config.height, GL_RGB, GL_FLOAT, pixels);

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
		windowWidth = newWidth;
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
			case 's': {
				// Write image to PPM file
				std::ofstream f("image.ppm", std::ofstream::trunc);
				if (!f.good()) {
					OCLTOY_LOG("Failed to open image file: image.ppm");
				} else {
					f << "P3" << std::endl;
					f << config.width << " " << config.height << std::endl;
					f << "255" << std::endl;

					for (int y = (int)config.height - 1; y >= 0; --y) {
						const unsigned char *p = (unsigned char *)(&pixels[y * config.width]);
						for (int x = 0; x < (int)config.width; ++x, p++) {
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
			UpdateJulia();
			glutPostRedisplay();
		}
	}

	void SpecialCallBack(int key, int x, int y) {
	}

	void MouseCallBack(int button, int state, int x, int y) {
	}

	virtual void MotionCallBack(int x, int y) {
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
			program.build(buildDevice,"-I.");
		} catch (cl::Error err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
			OCLTOY_LOG("Kernel compilation error:\n" << strError.c_str());

			throw err;
		}

		kernelJulia = cl::Kernel(program, "JuliaGPU");
		kernelJulia.getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &workGroupSize);
		if (commandLineOpts.count("workgroupsize"))
			workGroupSize = commandLineOpts["workgroupsize"].as<size_t>();
		OCLTOY_LOG("Using workgroup size: " << workGroupSize);
	}

	void FreeBuffers() {
		FreeOCLBuffer(0, &pixelsBuff);
		delete[] pixels;

		FreeOCLBuffer(0, &configBuff);
	}

	void AllocateBuffers() {
		delete[] pixels;
		const int pixelCount = config.width * config.height;
		const size_t size = pixelCount;
		pixels = new float[size * 3];
		std::fill(&pixels[0], &pixels[size * 3], 0);

		AllocOCLBufferWO(0, &pixelsBuff, size * sizeof(float) * 3, "FrameBuffer");

		AllocOCLBufferRO(0, &configBuff, &config, sizeof(RenderingConfig), "RenderingConfig");
	}

	void UpdateJulia() {
		const double startTime = WallClockTime();

		// Set kernel arguments
		kernelJulia.setArg(0, *pixelsBuff);
		kernelJulia.setArg(1, *configBuff);
		kernelJulia.setArg(2, 0);
		kernelJulia.setArg(3, 0.f);
		kernelJulia.setArg(4, 0.f);

		// Enqueue a kernel run
		size_t globalThreads = config.width * config.height;
		if (globalThreads % workGroupSize != 0)
			globalThreads = (globalThreads / workGroupSize + 1) * workGroupSize;

		cl::CommandQueue &oclQueue = deviceQueues[0];
		oclQueue.enqueueNDRangeKernel(kernelJulia, cl::NullRange,
				cl::NDRange(globalThreads), cl::NDRange(workGroupSize));

		// Read back the result
		oclQueue.enqueueReadBuffer(
				*pixelsBuff,
				CL_TRUE,
				0,
				pixelsBuff->getInfo<CL_MEM_SIZE>(),
				pixels);

		const double elapsedTime = WallClockTime() - startTime;
		const double sampleSec = config.width * config.height / elapsedTime;
		captionString = boost::str(boost::format("Rendering time: %.3f secs (Sample/sec %.1fK)") %
				elapsedTime % (sampleSec / 1000.0));
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
		PrintString(GLUT_BITMAP_HELVETICA_18, "s - save image.ppm");

		glDisable(GL_BLEND);
	}

	int mouseButton0, mouseButton2, mouseGrabLastX, mouseGrabLastY;

	float *pixels;
	cl::Buffer *pixelsBuff;

	RenderingConfig config;
	cl::Buffer *configBuff;

	cl::Kernel kernelJulia;
	size_t workGroupSize;

	std::string captionString;
};

int main(int argc, char **argv) {
	JuliaGPU toy;
	return toy.Run(argc, argv);
}
