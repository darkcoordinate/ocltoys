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

#include "opencl.h"
#include "utils.h"

#include <sstream>

extern void OCLToyDebugHandler(const char *msg);

#define OCLTOY_LOG(a) { std::stringstream _OCLTOY_LOG_LOCAL_SS; _OCLTOY_LOG_LOCAL_SS << a; OCLToyDebugHandler(_OCLTOY_LOG_LOCAL_SS.str().c_str()); }

class OCLToy {
public:
	OCLToy() { }
	virtual ~OCLToy() { }

	int Run(int argc, char *argv[]);

protected:
	virtual void ParseArgs() = 0;
	virtual int RunToy() = 0;

	int argc;
	char **argv;
};

#endif	/* OCLTOY_H */

