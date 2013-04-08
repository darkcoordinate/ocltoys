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

#if defined(WIN32)
// Requires for M_PI
#define _USE_MATH_DEFINES
#endif

#include "ocltoy.h"
#include "camera.h"
#include "geom.h"

#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>

#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

class SmallPTGPU : public OCLToy {
public:
	SmallPTGPU() : OCLToy("SmallPTGPU v" OCLTOYS_VERSION_MAJOR "." OCLTOYS_VERSION_MINOR " (OCLToys: http://code.google.com/p/ocltoys)"),
			samplesBuff(NULL), pixelsBuff(NULL), seedsBuff(NULL), cameraBuff(NULL),
			spheresBuff(NULL), kernelWorkGroupSize(64), pixels(NULL), currentSample(0), currentSphere(0) {
		useIdleCallback = true;
		kernelIterations = 1;
		sampleSec = 0.0;
		lastUserInputTime = WallClockTime();
	}

	virtual ~SmallPTGPU() {
		FreeBuffers();
	}

protected:
	boost::program_options::options_description GetOptionsDescriction() {
		boost::program_options::options_description opts("SmallPTGPU options");

		opts.add_options()
			("kernel,k", boost::program_options::value<std::string>()->default_value("rendering_kernel.cl"),
				"OpenCL kernel file name")
			("scene,n", boost::program_options::value<std::string>()->default_value("scenes/cornell.scn"),
				"Filename of the scene to render")
			("workgroupsize,z", boost::program_options::value<size_t>(), "OpenCL workgroup size");

		return opts;
	}

	virtual int RunToy() {
		ReadScene(commandLineOpts["scene"].as<std::string>());

		SetUpOpenCL();
		UpdateCameraBuffer();
		UpdateRender();

		glutMainLoop();

		return EXIT_SUCCESS;
	}

	//--------------------------------------------------------------------------
	// GLUT related code
	//--------------------------------------------------------------------------

	virtual void DisplayCallBack() {
		UpdateRender();

		glClear(GL_COLOR_BUFFER_BIT);
		glRasterPos2i(0, 0);
		glDrawPixels(windowWidth, windowHeight, GL_RGB, GL_FLOAT, pixels);

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

	//--------------------------------------------------------------------------
	// GLUT call backs
	//--------------------------------------------------------------------------

	virtual void ReshapeCallBack(int newWidth, int newHeight) {
		windowWidth = newWidth;
		windowHeight = newHeight;

		glViewport(0, 0, windowWidth, windowHeight);
		glLoadIdentity();
		glOrtho(0.f, windowWidth - 1.f,
				0.f, windowHeight - 1.f, -1.f, 1.f);

		UpdateCamera();
		UpdateCameraBuffer();
		ResizeFrameBuffer();
		UpdateKernelsArgs();

		glutPostRedisplay();
		lastUserInputTime = WallClockTime();
	}

#define MOVE_STEP 0.5f
#define ROTATE_STEP (2.f * ((float)M_PI) / 180.f)
	virtual void KeyCallBack(unsigned char key, int x, int y) {
		bool cameraUpdated = false;
		bool sceneUpdated = false;
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

					for (int y = (int)windowHeight - 1; y >= 0; --y) {
						const float *p = &pixels[y * windowWidth * 3];
						for (int x = 0; x < (int)windowWidth; ++x) {
							const float rv = std::min(std::max(*p++, 0.f), 1.f);
							const std::string r = boost::lexical_cast<std::string>((int)(rv * 255.f + .5f));
							const float gv = std::min(std::max(*p++, 0.f), 1.f);
							const std::string g = boost::lexical_cast<std::string>((int)(gv * 255.f + .5f));
							const float bv = std::min(std::max(*p++, 0.f), 1.f);
							const std::string b = boost::lexical_cast<std::string>((int)(bv * 255.f + .5f));
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
			case ' ': // Restart rendering
				currentSample = 0;
				break;
			case 'h':
				printHelp = (!printHelp);
				break;
			case 'a': {
				Vec dir = camera.x;
				vnorm(dir);
				vsmul(dir, -MOVE_STEP, dir);
				vadd(camera.orig, camera.orig, dir);
				vadd(camera.target, camera.target, dir);
				cameraUpdated = true;
				break;
			}
			case 'd': {
				Vec dir = camera.x;
				vnorm(dir);
				vsmul(dir, MOVE_STEP, dir);
				vadd(camera.orig, camera.orig, dir);
				vadd(camera.target, camera.target, dir);
				cameraUpdated = true;
				break;
			}
			case 'w': {
				Vec dir = camera.dir;
				vsmul(dir, MOVE_STEP, dir);
				vadd(camera.orig, camera.orig, dir);
				vadd(camera.target, camera.target, dir);
				cameraUpdated = true;
				break;
			}
			case 's': {
				Vec dir = camera.dir;
				vsmul(dir, -MOVE_STEP, dir);
				vadd(camera.orig, camera.orig, dir);
				vadd(camera.target, camera.target, dir);
				cameraUpdated = true;
				break;
			}
			case 'r': {
				camera.orig.y += MOVE_STEP;
				camera.target.y += MOVE_STEP;
				cameraUpdated = true;
				break;
			}
			case 'f': {
				camera.orig.y -= MOVE_STEP;
				camera.target.y -= MOVE_STEP;
				cameraUpdated = true;
				break;
			}
			case '+':
				currentSphere = (currentSphere + 1) % spheres.size();
				OCLTOY_LOG("Selected sphere " << currentSphere << " (" <<
						spheres[currentSphere].p.x << " " <<
						spheres[currentSphere].p.y << " " <<
						spheres[currentSphere].p.z << ")");
				sceneUpdated = true;
				break;
			case '-':
				currentSphere = (currentSphere + (spheres.size() - 1)) % (unsigned int)spheres.size();
				OCLTOY_LOG("Selected sphere " << currentSphere << " (" <<
						spheres[currentSphere].p.x << " " <<
						spheres[currentSphere].p.y << " " <<
						spheres[currentSphere].p.z << ")");
				sceneUpdated = true;
				break;
			case '4':
				spheres[currentSphere].p.x -= 0.5f * MOVE_STEP;
				sceneUpdated = true;
				break;
			case '6':
				spheres[currentSphere].p.x += 0.5f * MOVE_STEP;
				sceneUpdated = true;
				break;
			case '8':
				spheres[currentSphere].p.z -= 0.5f * MOVE_STEP;
				sceneUpdated = true;
				break;
			case '2':
				spheres[currentSphere].p.z += 0.5f * MOVE_STEP;
				sceneUpdated = true;
				break;
			case '9':
				spheres[currentSphere].p.y += 0.5f * MOVE_STEP;
				sceneUpdated = true;
				break;
			case '3':
				spheres[currentSphere].p.y -= 0.5f * MOVE_STEP;
				sceneUpdated = true;
				break;
			default:
				needRedisplay = false;
				break;
		}

		if (cameraUpdated) {
			UpdateCamera();
			UpdateCameraBuffer();
			// Restart the rendering
			currentSample = 0;
		}

		if (sceneUpdated) {
			UpdateSpheresBuffer();
			// Restart the rendering
			currentSample = 0;
		}

		if (needRedisplay) {
			glutPostRedisplay();
			lastUserInputTime = WallClockTime();
		}
	}

	void SpecialCallBack(int key, int x, int y) {
		bool cameraUpdated = false;
		bool needRedisplay = true;

        switch (key) {
			case GLUT_KEY_UP: {
				Vec t = camera.target;
				vsub(t, t, camera.orig);
				t.y = t.y * cosf(-ROTATE_STEP) + t.z * sinf(-ROTATE_STEP);
				t.z = -t.y * sinf(-ROTATE_STEP) + t.z * cosf(-ROTATE_STEP);
				vadd(t, t, camera.orig);
				camera.target = t;
				cameraUpdated = true;
				break;
			}
			case GLUT_KEY_DOWN: {
				Vec t = camera.target;
				vsub(t, t, camera.orig);
				t.y = t.y * cosf(ROTATE_STEP) + t.z * sinf(ROTATE_STEP);
				t.z = -t.y * sinf(ROTATE_STEP) + t.z * cosf(ROTATE_STEP);
				vadd(t, t, camera.orig);
				camera.target = t;
				cameraUpdated = true;
				break;
			}
			case GLUT_KEY_LEFT: {
				Vec t = camera.target;
				vsub(t, t, camera.orig);
				t.x = t.x * cosf(-ROTATE_STEP) - t.z * sinf(-ROTATE_STEP);
				t.z = t.x * sinf(-ROTATE_STEP) + t.z * cosf(-ROTATE_STEP);
				vadd(t, t, camera.orig);
				camera.target = t;
				cameraUpdated = true;
				break;
			}
			case GLUT_KEY_RIGHT: {
				Vec t = camera.target;
				vsub(t, t, camera.orig);
				t.x = t.x * cosf(ROTATE_STEP) - t.z * sinf(ROTATE_STEP);
				t.z = t.x * sinf(ROTATE_STEP) + t.z * cosf(ROTATE_STEP);
				vadd(t, t, camera.orig);
				camera.target = t;
				cameraUpdated = true;
				break;
			}
			case GLUT_KEY_PAGE_UP:
				camera.target.y += MOVE_STEP;
				cameraUpdated = true;
				break;
			case GLUT_KEY_PAGE_DOWN:
				camera.target.y -= MOVE_STEP;
				cameraUpdated = true;
				break;
			default:
				break;
		}

		if (cameraUpdated) {
			UpdateCamera();
			UpdateCameraBuffer();
			// Restart the rendering
			currentSample = 0;
		}

		if (needRedisplay) {
			glutPostRedisplay();
			lastUserInputTime = WallClockTime();
		}
	}

	void MouseCallBack(int button, int state, int x, int y) {
	}

	virtual void MotionCallBack(int x, int y) {
		bool needRedisplay = true;

		if (needRedisplay) {
			glutPostRedisplay();
			lastUserInputTime = WallClockTime();
		}
	}

	void IdleCallBack() {
		glutPostRedisplay();
	}

	//--------------------------------------------------------------------------
	// OpenCL related code
	//--------------------------------------------------------------------------

	virtual unsigned int GetMaxDeviceCountSupported() const { return 1; }

private:
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
		cl::Program::Sources source(1, std::make_pair(kernelSource.c_str(), kernelSource.length()));
		cl::Program program = cl::Program(oclContext, source);
		try {
			VECTOR_CLASS<cl::Device> buildDevice;
			buildDevice.push_back(oclDevice);
			program.build(buildDevice,"-I. -I../common");
		} catch (cl::Error err) {
			cl::STRING_CLASS strError = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(oclDevice);
			OCLTOY_LOG("Kernel compilation error:\n" << strError.c_str());

			throw err;
		}

		kernelSmallPT = cl::Kernel(program, "SmallPTGPU");
		kernelSmallPT.getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, &kernelWorkGroupSize);
		if (commandLineOpts.count("workgroupsize"))
			kernelWorkGroupSize = commandLineOpts["workgroupsize"].as<size_t>();
		OCLTOY_LOG("Using workgroup size: " << kernelWorkGroupSize);

		kernelToneMapping = cl::Kernel(program, "ToneMapping");
		
		//----------------------------------------------------------------------
		// Allocate buffer
		//----------------------------------------------------------------------

		AllocateBuffers();

		//----------------------------------------------------------------------
		// Set kernel arguments
		//----------------------------------------------------------------------

		UpdateKernelsArgs();
	}

	void FreeBuffers() {
		FreeOCLBuffer(0, &samplesBuff);
		FreeOCLBuffer(0, &pixelsBuff);
		delete[] pixels;
		FreeOCLBuffer(0, &seedsBuff);
		FreeOCLBuffer(0, &cameraBuff);
		FreeOCLBuffer(0, &spheresBuff);
	}

	void AllocateBuffers() {
		AllocOCLBufferRO(0, &cameraBuff, &camera, sizeof(Camera), "CameraBuffer");
		AllocOCLBufferRO(0, &spheresBuff, &spheres[0], sizeof(Sphere) * spheres.size(), "SpheresBuffer");

		// Allocate the frame buffer
		ResizeFrameBuffer();
	}

	void ReadScene(const std::string &fileName) {
		OCLTOY_LOG("Reading scene: " << fileName);

		std::ifstream f(fileName.c_str(), std::ifstream::in | std::ifstream::binary);
		if (!f.good())
			throw std::runtime_error("Failed to open file: " + fileName);

		// Read the camera position
		std::string cameraLine;
		std::getline(f, cameraLine);
		if (!f.good())
			throw std::runtime_error("Failed to read camera parameters");

		std::vector<std::string> cameraArgs;
		boost::split(cameraArgs, cameraLine, boost::is_any_of("\t "));
		if (cameraArgs.size() != 7)
			throw std::runtime_error("Failed to parse 6 camera parameters");

		camera.orig.x = static_cast<float>(boost::lexical_cast<double>(cameraArgs[1]));
		camera.orig.y = static_cast<float>(boost::lexical_cast<double>(cameraArgs[2]));
		camera.orig.z = static_cast<float>(boost::lexical_cast<double>(cameraArgs[3]));
		camera.target.x = static_cast<float>(boost::lexical_cast<double>(cameraArgs[4]));
		camera.target.y = static_cast<float>(boost::lexical_cast<double>(cameraArgs[5]));
		camera.target.z = static_cast<float>(boost::lexical_cast<double>(cameraArgs[6]));
		UpdateCamera();

		// Read the sphere count
		std::string sizeLine;
		std::getline(f, sizeLine);
		if (!f.good())
			throw std::runtime_error("Failed to read sphere count");

		std::vector<std::string> sizeArgs;
		boost::split(sizeArgs, sizeLine, boost::is_any_of("\t "));
		if (sizeArgs.size() != 2)
			throw std::runtime_error("Failed to parse sphere count");
		
		const unsigned int sphereCount = boost::lexical_cast<unsigned int>(sizeArgs[1]);
		spheres.resize(sphereCount);

		// Read all spheres
		for (unsigned int i = 0; i < sphereCount; i++) {
			// Read the sphere definition
			std::string sphereLine;
			std::getline(f, sphereLine);
			if (!f.good())
				throw std::runtime_error("Failed to read sphere #" + boost::lexical_cast<int>(i));

			std::vector<std::string> sphereArgs;
			boost::split(sphereArgs, sphereLine, boost::is_any_of("\t "));
			if (sphereArgs.size() != 12)
				throw std::runtime_error("Failed to parse sphere #" + boost::lexical_cast<int>(i));

			Sphere *s = &spheres[i];
			s->rad = static_cast<float>(boost::lexical_cast<double>(sphereArgs[1]));
			s->p.x = static_cast<float>(boost::lexical_cast<double>(sphereArgs[2]));
			s->p.y = static_cast<float>(boost::lexical_cast<double>(sphereArgs[3]));
			s->p.z = static_cast<float>(boost::lexical_cast<double>(sphereArgs[4]));
			s->e.x = static_cast<float>(boost::lexical_cast<double>(sphereArgs[5]));
			s->e.y = static_cast<float>(boost::lexical_cast<double>(sphereArgs[6]));
			s->e.z = static_cast<float>(boost::lexical_cast<double>(sphereArgs[7]));
			s->c.x = static_cast<float>(boost::lexical_cast<double>(sphereArgs[8]));
			s->c.y = static_cast<float>(boost::lexical_cast<double>(sphereArgs[9]));
			s->c.z = static_cast<float>(boost::lexical_cast<double>(sphereArgs[10]));

			const unsigned int material = boost::lexical_cast<int>(sphereArgs[11]);
			switch (material) {
				case 0:
					s->refl = DIFF;
					break;
				case 1:
					s->refl = SPEC;
					break;
				case 2:
					s->refl = REFR;
					break;
				default:
					throw std::runtime_error("Unknown material for sphere #" + boost::lexical_cast<int>(i));
			}
		}

		f.close();
	}

	void UpdateCamera() {
		vsub(camera.dir, camera.target, camera.orig);
		vnorm(camera.dir);

		const Vec up = {0.f, 1.f, 0.f};
		const float fov = (FLOAT_PI / 180.f) * 45.f;
		vxcross(camera.x, camera.dir, up);
		vnorm(camera.x);
		vsmul(camera.x, windowWidth * fov / windowHeight, camera.x);

		vxcross(camera.y, camera.x, camera.dir);
		vnorm(camera.y);
		vsmul(camera.y, fov, camera.y);
	}

	void ResizeFrameBuffer() {
		cl::CommandQueue &oclQueue = deviceQueues[0];
		const size_t pixelCount = windowWidth * windowHeight;

		// Allocate the sample buffer
		AllocOCLBufferRW(0, &samplesBuff, pixelCount * sizeof(float) * 3, "SamplesBuffer");

		// Allocate the frame buffer
		delete[] pixels;
		pixels = new float[pixelCount * 3];
		std::fill(&pixels[0], &pixels[pixelCount * 3], 0.f);
		AllocOCLBufferWO(0, &pixelsBuff, pixelCount * sizeof(float) * 3, "PixelsBuffer");

		// Allocate the seeds for random number generator
		AllocOCLBufferRW(0, &seedsBuff, pixelCount * sizeof(unsigned int) * 2, "SeedsBuffer");

		unsigned int *seeds = new unsigned int[pixelCount * 2];
		for (size_t i = 0; i < pixelCount * 2; i++) {
			seeds[i] = rand();
			if (seeds[i] < 2)
				seeds[i] = 2;
		}
		oclQueue.enqueueWriteBuffer(*seedsBuff,
				CL_TRUE,
				0,
				seedsBuff->getInfo<CL_MEM_SIZE>(),
				seeds);
		delete[] seeds;

		// Better to restart load balancing
		kernelIterations = 1;
		currentSample = 0;
	}

	void UpdateKernelsArgs() {
		kernelSmallPT.setArg(0, *samplesBuff);
		kernelSmallPT.setArg(1, *seedsBuff);
		kernelSmallPT.setArg(2, *cameraBuff);
		kernelSmallPT.setArg(3, (unsigned int)spheres.size());
		kernelSmallPT.setArg(4, *spheresBuff);
		kernelSmallPT.setArg(5, windowWidth);
		kernelSmallPT.setArg(6, windowHeight);
		kernelSmallPT.setArg(7, currentSample);

		kernelToneMapping.setArg(0, *samplesBuff);
		kernelToneMapping.setArg(1, *pixelsBuff);
		kernelToneMapping.setArg(2, windowWidth);
		kernelToneMapping.setArg(3, windowHeight);
	}

	void UpdateCameraBuffer() {
		cl::CommandQueue &oclQueue = deviceQueues[0];
		oclQueue.enqueueWriteBuffer(*cameraBuff,
				CL_FALSE,
				0,
				cameraBuff->getInfo<CL_MEM_SIZE>(),
				&camera);
	}

	void UpdateSpheresBuffer() {
		cl::CommandQueue &oclQueue = deviceQueues[0];
		oclQueue.enqueueWriteBuffer(*spheresBuff,
				CL_FALSE,
				0,
				spheresBuff->getInfo<CL_MEM_SIZE>(),
				&spheres[0]);		
	}

	void UpdateRender() {
		const double startTime = WallClockTime();

		size_t globalThreads = windowWidth * windowHeight;
		if (globalThreads % kernelWorkGroupSize != 0)
			globalThreads = (globalThreads / kernelWorkGroupSize + 1) * kernelWorkGroupSize;

		cl::CommandQueue &oclQueue = deviceQueues[0];
		for (unsigned int i = 0; i < kernelIterations; ++i) {
			// Set kernel arguments
			kernelSmallPT.setArg(7, currentSample++);

			// Enqueue a kernel run
			oclQueue.enqueueNDRangeKernel(kernelSmallPT, cl::NullRange,
					cl::NDRange(globalThreads), cl::NDRange(kernelWorkGroupSize));
		}

		// Image tone mapping
		oclQueue.enqueueNDRangeKernel(kernelToneMapping, cl::NullRange,
					cl::NDRange(globalThreads), cl::NDRange(kernelWorkGroupSize));

		// Read back the result
		oclQueue.enqueueReadBuffer(
				*pixelsBuff,
				CL_TRUE,
				0,
				pixelsBuff->getInfo<CL_MEM_SIZE>(),
				pixels);

		const double elapsedTime = WallClockTime() - startTime;
		// A simple trick to smooth sample/sec value
		const double k = 0.1;
		sampleSec = sampleSec * (1.0 - k) + k * (kernelIterations * windowWidth * windowHeight / elapsedTime);
		captionString = boost::str(boost::format("[Pass %d][Rendering time (%d iterations per frame): %.3f secs (%.1fM Sample/sec)]") %
				(currentSample + 1) % kernelIterations % elapsedTime % (sampleSec / 1000000.0));

		if (elapsedTime < 0.075) {
			// Too fast, increase the number of kernel iterations
			++kernelIterations;
		} else if (elapsedTime > 0.1) {
			// Too slow, decrease the number of kernel iterations
			kernelIterations = std::max(kernelIterations - 1u, 1u);
		}
	}

	void PrintHelp() {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(0.f, 0.f, 0.5f, 0.5f);
		glRecti(40, 40, 600, 440);

		glColor3f(1.f, 1.f, 1.f);
		glRasterPos2i(300, 420);
		PrintString(GLUT_BITMAP_HELVETICA_18, "Help");

        // Help
		glRasterPos2i(60, 390);
		PrintString(GLUT_BITMAP_9_BY_15, "h - toggle Help");
		glRasterPos2i(60, 375);
		PrintString(GLUT_BITMAP_9_BY_15, "arrow Keys - rotate camera left/right/up/down");
		glRasterPos2i(60, 360);
		PrintString(GLUT_BITMAP_9_BY_15, "a and d - move camera left and right");
		glRasterPos2i(60, 345);
		PrintString(GLUT_BITMAP_9_BY_15, "w and s - move camera forward and backward");
		glRasterPos2i(60, 330);
		PrintString(GLUT_BITMAP_9_BY_15, "r and f - move camera up and down");
		glRasterPos2i(60, 315);
		PrintString(GLUT_BITMAP_9_BY_15, "PageUp and PageDown - move camera target up and down");
		glRasterPos2i(60, 300);
		PrintString(GLUT_BITMAP_9_BY_15, "+ and - - to select next/previous object");
		glRasterPos2i(60, 285);
		PrintString(GLUT_BITMAP_9_BY_15, "2, 3, 4, 5, 6, 8, 9 - to move selected object");

		glDisable(GL_BLEND);
	}

	double lastUserInputTime;

	cl::Buffer *samplesBuff;
	cl::Buffer *pixelsBuff;
	cl::Buffer *seedsBuff;
	cl::Buffer *cameraBuff;
	cl::Buffer *spheresBuff;

	cl::Kernel kernelSmallPT, kernelToneMapping;
	size_t kernelWorkGroupSize;
	unsigned int kernelIterations;

	float *pixels;
	Camera camera;
	std::vector<Sphere> spheres;

	double sampleSec;
	unsigned int currentSample, currentSphere;
	std::string captionString;
};

int main(int argc, char **argv) {
	SmallPTGPU toy;
	return toy.Run(argc, argv);
}
