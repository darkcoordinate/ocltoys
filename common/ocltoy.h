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

#ifndef OCLTOY_H
#define	OCLTOY_H

#include "utils.h"
#include "version.h"

#include <sstream>

#include <boost/program_options.hpp>

extern void OCLToyDebugHandler(const char *msg);

#define OCLTOY_LOG(a) { std::stringstream _OCLTOY_LOG_LOCAL_SS; _OCLTOY_LOG_LOCAL_SS << a; OCLToyDebugHandler(_OCLTOY_LOG_LOCAL_SS.str().c_str()); }

class OCLToy {
public:
	OCLToy(const std::string &winTitle);
	virtual ~OCLToy() { }

	virtual int Run(int argc, char *argv[]);

protected:
	virtual void ReshapeCallBack(int newWidth, int newHeight);
	virtual void DisplayCallBack() { }
	virtual void TimerCallBack(int value) { }
	virtual void KeyCallBack(unsigned char key, int x, int y) { }
	virtual void SpecialCallBack(int key, int x, int y) { }
	virtual void MouseCallBack(int button, int state, int x, int y) { }
	virtual void MotionCallBack(int x, int y) { }

	virtual void InitGlut();

	virtual boost::program_options::options_description GetOptionsDescriction() = 0;
	virtual int RunToy() = 0;

	boost::program_options::variables_map commandLineOpts;

	std::string windowTitle;
	int windowWidth, windowHeight;

	bool printHelp;

private:
	// It is possible to run only a single Toy at time
	static OCLToy *currentOCLToy;
	static void GlutReshapeFunc(int newWidth, int newHeight);
	static void GlutDisplayFunc();
	static void GlutTimerFunc(int value);
	static void GlutKeyFunc(unsigned char key, int x, int y);
	static void GlutSpecialFunc(int key, int x, int y);
	static void GlutMouseFunc(int button, int state, int x, int y);
	static void GlutMotionFunc(int x, int y);
};

#endif	/* OCLTOY_H */

