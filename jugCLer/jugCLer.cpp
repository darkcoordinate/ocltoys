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

#include <fstream>
#include <iostream>

#include "ocltoy.h"

#include "scene.h"
#include "animation.h"

struct PixelRGBA8888 {
  unsigned char r, g, b, a;
};

struct Bitmap {
  PixelRGBA8888* pixels;
  int width;
  int height;
  
  Bitmap(int Width, int Height) {
    width = Width;
    height = Height;
    pixels = new PixelRGBA8888[Width * Height];
    if (pixels == NULL)
		throw std::runtime_error("Out of memory for pixels");
  }
  
  ~Bitmap() {
    delete[] pixels;
  }

  void draw(void) {
    glRasterPos2i(0, 0);
    glDrawPixels(width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  }
};

class JugCLer : public OCLToy {
public:
	JugCLer() : OCLToy("JugCLer v" OCLTOYS_VERSION_MAJOR "." OCLTOYS_VERSION_MINOR " (OCLToys: http://code.google.com/p/ocltoys)") {
		millisTimerFunc = 1000.0 / 60.0; // 60Hz

		bitmap = NULL;
		scene = NULL;
		pixelsBuff = NULL;
		sceneBuff = NULL;

		frameSec = 0.0;
	}

	virtual ~JugCLer() {
		FreeBuffers();
		delete bitmap;
		delete scene;
	}

protected:
	boost::program_options::options_description GetOptionsDescriction() {
		boost::program_options::options_description opts("JugCLer options");

		opts.add_options()
			("kernel,k", boost::program_options::value<std::string>()->default_value("trace.cl"),
				"OpenCL kernel file name")
			("workgroupsize,z", boost::program_options::value<size_t>(), "OpenCL workgroup size");

		return opts;
	}

	virtual int RunToy() {
		// set up buffers
		bitmap = new Bitmap(windowWidth, windowHeight);
		scene = new Scene;

		// set up scene
		setupAnim(scene, windowWidth, windowHeight);

		SetUpOpenCL();

		glutMainLoop();

		return EXIT_SUCCESS;
	}

	//--------------------------------------------------------------------------
	// GLUT related code
	//--------------------------------------------------------------------------

	virtual void DisplayCallBack() {
		ComputeImage();
		bitmap->draw();

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(0.f, 0.f, 0.f, 0.8f);
		glRecti(0, windowHeight - 15,
				windowWidth - 1, windowHeight - 1);
		glRecti(0, 0, windowWidth - 1, 18);
		glDisable(GL_BLEND);

		// Title
		glColor3f(1.f, 1.f, 1.f);
		glRasterPos2i(4, windowHeight - 10);
		PrintString(GLUT_BITMAP_8_BY_13, "jugCLer 1.0 by Holger Bettag (hobold@vectorizer.org)");
		glRasterPos2i(4, windowHeight - 20);
		PrintString(GLUT_BITMAP_8_BY_13, "Heavily inspired by Michael Birken's"
				" reverse engineering of Eric Graham's original Amiga"
				" juggler demo");

//		// Caption line 0
//		double globalSampleSec = 0.0;
//		unsigned int globalPass = 0;
//		for (unsigned int i = 0; i < selectedDevices.size(); ++i) {
//			globalSampleSec += sampleSec[i];
//			globalPass += currentSample[i] + 1;
//		}
//		
//		const std::string captionString = boost::str(boost::format("[Pass %d][%.1fM Sample/sec]") %
//				globalPass % (globalSampleSec / 1000000.0));
//
//		glColor3f(1.f, 1.f, 1.f);
//		glRasterPos2i(4, 5);
//		PrintString(GLUT_BITMAP_8_BY_13, captionString.c_str());

		if (printHelp) {
			glPushMatrix();
			glLoadIdentity();
			glOrtho(-.5f, windowWidth - .5f,
				-.5f, windowHeight - .5f, -1.f, 1.f);

			PrintHelp();

			glPopMatrix();
		}

		glutSwapBuffers();
	}

	//--------------------------------------------------------------------------
	// GLUT call backs
	//--------------------------------------------------------------------------

	virtual void ReshapeCallBack(int newWidth, int newHeight) {
		windowWidth = newWidth;
		windowHeight = newHeight;

		glViewport(0, 0, windowWidth, windowHeight);
		glLoadIdentity();
		glOrtho(-.5f, windowWidth - .5f,
				-.5f, windowHeight - .5f, -1.f, 1.f);

		delete bitmap;
		bitmap = new Bitmap(windowWidth, windowHeight);
		const unsigned int pixelCount = windowWidth * windowHeight;
		AllocOCLBufferWO(0, &pixelsBuff, pixelCount * sizeof(PixelRGBA8888), "PixelsBuffer");
		kernelsJugCLer.setArg(1, *pixelsBuff);

		setupAnim(scene, windowWidth, windowHeight);

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
						const PixelRGBA8888 *p = &bitmap->pixels[y * windowWidth];
						for (int x = 0; x < windowWidth; ++x, p++) {
							const std::string r = boost::lexical_cast<std::string>(p->r);
							const std::string g = boost::lexical_cast<std::string>(p->g);
							const std::string b = boost::lexical_cast<std::string>(p->b);
							f << r << " " << g << " " << b << std::endl;
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
			default:
				needRedisplay = false;
				break;
		}

		if (needRedisplay)
			glutPostRedisplay();
	}

	void SpecialCallBack(int key, int x, int y) {
	}

	void TimerCallBack(int id) {
		animatePositions(scene);

		glutPostRedisplay();

		// Refresh the timer
		glutTimerFunc(millisTimerFunc, &OCLToy::GlutTimerFunc, 0);
	}

	//--------------------------------------------------------------------------
	// OpenCL related code
	//--------------------------------------------------------------------------

	virtual unsigned int GetMaxDeviceCountSupported() const {
		return 1;
	}

private:
	// compute and draw image
	void ComputeImage() {
		const double t0 = WallClockTime();

		// copy scene from host to device
		cl::CommandQueue &oclQueue = deviceQueues[0];
		oclQueue.enqueueWriteBuffer(*sceneBuff,
				CL_FALSE,
				0,
				sceneBuff->getInfo<CL_MEM_SIZE>(),
				scene);

		size_t globalThreads = windowWidth * windowHeight;
		if (globalThreads % kernelsWorkGroupSize != 0)
			globalThreads = (globalThreads / kernelsWorkGroupSize + 1) * kernelsWorkGroupSize;

		// Enqueue a kernel run
		oclQueue.enqueueNDRangeKernel(kernelsJugCLer, cl::NullRange,
				cl::NDRange(globalThreads), cl::NDRange(kernelsWorkGroupSize));

		// Read back the result
		oclQueue.enqueueReadBuffer(
				*(pixelsBuff),
				CL_FALSE,
				0,
				pixelsBuff->getInfo<CL_MEM_SIZE>(),
				bitmap->pixels);
		oclQueue.finish();
		const double t1 = WallClockTime();

		// A simple trick to smooth sample/sec value
		const double k = 0.05;
		frameSec = (1.0 - k) * frameSec + k * (1.0 / (t1 -t0));
	}

	void SetUpOpenCL() {
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
		cl::Program program = cl::Program(oclContext, kernelSource);
		try {
			VECTOR_CLASS<cl::Device> buildDevice;
			buildDevice.push_back(oclDevice);
			program.build(buildDevice);
		} catch (cl::Error err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
			OCLTOY_LOG("Kernel compilation error:\n" << strError.c_str());

			throw err;
		}

		kernelsJugCLer = cl::Kernel(program, "render_gpu");
		kernelsJugCLer.getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &kernelsWorkGroupSize);
		if (commandLineOpts.count("workgroupsize"))
			kernelsWorkGroupSize = commandLineOpts["workgroupsize"].as<size_t>();
		OCLTOY_LOG("Using workgroup size: " << kernelsWorkGroupSize);

		//----------------------------------------------------------------------
		// Allocate buffer
		//----------------------------------------------------------------------

		AllocateBuffers();

		//----------------------------------------------------------------------
		// Set kernel arguments
		//----------------------------------------------------------------------

		kernelsJugCLer.setArg(0, *sceneBuff);
		kernelsJugCLer.setArg(1, *pixelsBuff);
	}

	void FreeBuffers() {
		FreeOCLBuffer(0, &pixelsBuff);
		FreeOCLBuffer(0, &sceneBuff);
	}

	void AllocateBuffers() {
		// Allocate the pixels buffer
		const unsigned int pixelCount = windowWidth * windowHeight;
		AllocOCLBufferWO(0, &pixelsBuff, pixelCount * sizeof(PixelRGBA8888), "PixelsBuffer");

		// Allocate the scene buffer
		AllocOCLBufferRO(0, &sceneBuff, scene, sizeof(Scene), "SceneBuffer");
	}

	void PrintHelp() {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(0.f, 0.f, 0.5f, 0.5f);
		glRecti(50, 50, windowWidth - 50, windowHeight - 50);

		glColor3f(1.f, 1.f, 1.f);
		int fontOffset = windowHeight - 50 - 20;
		glRasterPos2i((windowWidth - glutBitmapLength(GLUT_BITMAP_9_BY_15, (unsigned char *)"Help & Devices")) / 2, fontOffset);
		PrintString(GLUT_BITMAP_9_BY_15, "Help & Devices");

        // Help
		fontOffset -= 30;
		PrintHelpString(60, fontOffset, "h", "toggle Help");
		fontOffset -= 17;
		PrintHelpString(60, fontOffset, "arrow Keys", "rotate camera left/right/up/down");
		fontOffset -= 17;
		PrintHelpString(60, fontOffset, "a and d", "move camera left and right");
		fontOffset -= 17;
		PrintHelpString(60, fontOffset, "w and s", "move camera forward and backward");
		fontOffset -= 17;
		PrintHelpString(60, fontOffset, "r and f", "move camera up and down");
		fontOffset -= 17;
		PrintHelpString(60, fontOffset, "PageUp and PageDown", "move camera target up and down");
		fontOffset -= 17;
		PrintHelpString(60, fontOffset, "+ and -", "to select next/previous object");
		fontOffset -= 17;
		PrintHelpString(60, fontOffset, "2, 3, 4, 5, 6, 8, 9", "to move selected object");
		fontOffset -= 17;

		glDisable(GL_BLEND);
	}

	Bitmap *bitmap; // image for display
	Scene *scene; 

	cl::Buffer *pixelsBuff;
	cl::Buffer *sceneBuff;

	cl::Kernel kernelsJugCLer;
	size_t kernelsWorkGroupSize;
	
	double frameSec;
};

int main(int argc, char **argv) {
	JugCLer toy;
	return toy.Run(argc, argv);
}
