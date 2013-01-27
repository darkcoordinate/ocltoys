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

#include <iostream>
#include <stdexcept>
#include <string>

#include <boost/filesystem.hpp>

#include "ocltoy.h"

//------------------------------------------------------------------------------

#if defined(__GNUC__) && !defined(__CYGWIN__)
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <typeinfo>
#include <cxxabi.h>

static std::string Demangle(const char *symbol) {
	size_t size;
	int status;
	char temp[128];
	char* result;

	if (1 == sscanf(symbol, "%*[^'(']%*[^'_']%[^')''+']", temp)) {
		if (NULL != (result = abi::__cxa_demangle(temp, NULL, &size, &status))) {
			std::string r = result;
			return r + " [" + symbol + "]";
		}
	}

	if (1 == sscanf(symbol, "%127s", temp))
		return temp;

	return symbol;
}

void OCLToyTerminate(void) {
	OCLTOY_LOG("=========================================================");
	OCLTOY_LOG("Unhandled exception");

	void *array[32];
	size_t size = backtrace(array, 32);
	char **strings = backtrace_symbols(array, size);

	OCLTOY_LOG("Obtained " << size << " stack frames.");

	for (size_t i = 0; i < size; i++)
		OCLTOY_LOG("  " << Demangle(strings[i]));

	free(strings);
}
#endif

//------------------------------------------------------------------------------

OCLToy *OCLToy::currentOCLToy = NULL;

void OCLToyDebugHandler(const char *msg) {
	std::cerr << "[OCLToy] " << msg << std::endl;
}

OCLToy::OCLToy(const std::string &winTitle) : windowTitle(winTitle),
		windowWidth(800), windowHeight(600), printHelp(true) {
	currentOCLToy = this;
}

int OCLToy::Run(int argc, char **argv) {
#if defined(__GNUC__) && !defined(__CYGWIN__)
	std::set_terminate(OCLToyTerminate);
#endif

	try {
		OCLTOY_LOG(windowTitle);

		//----------------------------------------------------------------------
		// Parse command line options
		//----------------------------------------------------------------------

		glutInit(&argc, argv);

		boost::program_options::options_description genericOpts("Generic options");
		genericOpts.add_options()
			("help,h", "Display this help and exit")
			("width,w", boost::program_options::value<int>()->default_value(800), "Window width")
			("height,e", boost::program_options::value<int>()->default_value(600), "Window height")
			("directory,d", boost::program_options::value<std::string>(), "Current directory path")
			("noscreenhelp,s", "Disable on screen help");

		boost::program_options::options_description toyOpts = GetOptionsDescriction();

		boost::program_options::options_description opts;
		opts.add(genericOpts).add(toyOpts);

		try {
			// Disable guessing of option names
			const int cmdstyle = boost::program_options::command_line_style::default_style &
				~boost::program_options::command_line_style::allow_guessing;
			boost::program_options::store(boost::program_options::command_line_parser(argc, argv).
				style(cmdstyle).options(opts).run(), commandLineOpts);

			windowWidth = commandLineOpts["width"].as<int>();
			windowHeight = commandLineOpts["height"].as<int>();
			if (commandLineOpts.count("directory"))
				boost::filesystem::current_path(boost::filesystem::path(commandLineOpts["directory"].as<std::string>()));
			if (commandLineOpts.count("noscreenhelp"))
				printHelp = false;

			if (commandLineOpts.count("help")) {
				OCLTOY_LOG("Command usage" << std::endl << opts);
				exit(EXIT_SUCCESS);
			}
		} catch(boost::program_options::error &e) {
			OCLTOY_LOG("COMMAND LINE ERROR: " << e.what() << std::endl << opts); 
			exit(EXIT_FAILURE);
		}

		//----------------------------------------------------------------------
		// Run the application
		//----------------------------------------------------------------------

		return RunToy();
	} catch (cl::Error err) {
		OCLTOY_LOG("OpenCL ERROR: " << err.what() << "(" << OCLErrorString(err.err()) << ")");
		return EXIT_FAILURE;
	} catch (std::runtime_error err) {
		OCLTOY_LOG("RUNTIME ERROR: " << err.what());
		return EXIT_FAILURE;
	} catch (std::exception err) {
		OCLTOY_LOG("ERROR: " << err.what());
		return EXIT_FAILURE;
	}
}

void OCLToy::ReshapeCallBack(int newWidth, int newHeight) {
	// Check if width or height have really changed
	if ((newWidth != windowWidth) ||
			(newHeight != windowHeight)) {
		windowWidth = newWidth;
		windowHeight = newHeight;

		glViewport(0, 0, windowWidth, windowHeight);
		glLoadIdentity();
		glOrtho(0.f, windowWidth - 1.f,
				0.f, windowHeight - 1.f, -1.f, 1.f);

		glutPostRedisplay();
	}
}

void OCLToy::GlutReshapeFunc(int newWidth, int newHeight) {
	currentOCLToy->ReshapeCallBack(newWidth, newHeight);
}

void OCLToy::GlutDisplayFunc() {
	currentOCLToy->DisplayCallBack();
}

void OCLToy::GlutTimerFunc(int value) {
	currentOCLToy->TimerCallBack(value);
}

void OCLToy::GlutKeyFunc(unsigned char key, int x, int y) {
	currentOCLToy->KeyCallBack(key, x, y);
}

void OCLToy::GlutSpecialFunc(int key, int x, int y) {
	currentOCLToy->SpecialCallBack(key, x, y);
}

void OCLToy::GlutMouseFunc(int button, int state, int x, int y) {
	currentOCLToy->MouseCallBack(button, state, x, y);
}

void OCLToy::GlutMotionFunc(int x, int y) {
	currentOCLToy->MotionCallBack(x, y);
}

void OCLToy::InitGlut() {
	glutInitWindowSize(windowWidth, windowHeight);
	// Center the window
	const int scrWidth = glutGet(GLUT_SCREEN_WIDTH);
	const int scrHeight = glutGet(GLUT_SCREEN_HEIGHT);
	if ((scrWidth + 50 < windowWidth) || (scrHeight + 50 < windowHeight))
		glutInitWindowPosition(0, 0);
	else
		glutInitWindowPosition((scrWidth - windowWidth) / 2, (scrHeight - windowHeight) / 2);

	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutCreateWindow(windowTitle.c_str());

	glutReshapeFunc(&OCLToy::GlutReshapeFunc);
	glutKeyboardFunc(&OCLToy::GlutKeyFunc);
	glutSpecialFunc(&OCLToy::GlutSpecialFunc);
	glutDisplayFunc(&OCLToy::GlutDisplayFunc);
	glutMouseFunc(&OCLToy::GlutMouseFunc);
	glutMotionFunc(&OCLToy::GlutMotionFunc);

	glMatrixMode(GL_PROJECTION);
	glViewport(0, 0, windowWidth, windowHeight);
	glLoadIdentity();
	glOrtho(0.f, windowWidth - 1.f,
			0.f, windowHeight - 1.f, -1.f, 1.f);
}
