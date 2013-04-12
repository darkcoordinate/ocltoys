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

// This code is based on the original Matias Piispanen's port of
// SmallPTGPU to WebCL

var MaterialType = {
	"MATTE" : 0, "MIRROR" : 1, "GLASS" : 2, "MATTETRANSLUCENT" : 3,
	"GLOSSY" : 4, "GLOSSYTRANSLUCENT" : 5
};

function Sphere() {
	this.rad = 1.0;
	
	this.p = vec3.create();
	this.e = vec3.create();
	
	this.material = MaterialType.MATTE;
	this.c = vec3.create();
}

Sphere.prototype.setMatte = function(r, p, e, c) {
  this.rad = r;
  this.p = p;
  this.e = e;
  this.material = MaterialType.MATTE;
  this.c = c;
};

Sphere.prototype.setMirror = function(r, p, e, c) {
  this.rad = r;
  this.p = p;
  this.e = e;
  this.material = MaterialType.MIRROR;
  this.c = c;
};

Sphere.prototype.setGlass = function(r, p, e, c, ior, sigmas, sigmaa) {
  this.rad = r;
  this.p = p;
  this.e = e;
  this.material = MaterialType.GLASS;
  this.c = c;
  this.ior = ior;
  this.sigmas = sigmas;
  this.sigmaa = sigmaa;
};

Sphere.prototype.setMatteTranslucent = function(r, p, e, c, transp, sigmas, sigmaa) {
  this.rad = r;
  this.p = p;
  this.e = e;
  this.material = MaterialType.MATTETRANSLUCENT;
  this.c = c;
  this.transparency = transp;
  this.sigmas = sigmas;
  this.sigmaa = sigmaa;
};

Sphere.prototype.setMatteGlossy = function(r, p, e, c, exp) {
  this.rad = r;
  this.p = p;
  this.e = e;
  this.material = MaterialType.GLOSSY;
  this.c = c;
  this.exponent = exp;
};

Sphere.prototype.setMatteGlossyTranslucent = function(r, p, e, c, exp, transp, sigmas, sigmaa) {
  this.rad = r;
  this.p = p;
  this.e = e;
  this.material = MaterialType.GLOSSYTRANSLUCENT;
  this.c = c;
  this.exponent = exp;
  this.transparency = transp;
  this.sigmas = sigmas;
  this.sigmaa = sigmaa;
};
