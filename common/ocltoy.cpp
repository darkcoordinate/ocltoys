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

#include "ocltoy.h"

void OCLToyDebugHandler(const char *msg) {
	std::cerr << "[OCLToy] " << msg << std::endl;
}

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

int OCLToy::Run(int ac, char **av) {
#if defined(__GNUC__) && !defined(__CYGWIN__)
	std::set_terminate(OCLToyTerminate);
#endif

	argc = ac;
	argv = av;

	try {
		ParseArgs();

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