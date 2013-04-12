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

var WALL_RAD = 10000;

function Scene(spheres) {
	this.spheres = [];
	if(spheres == undefined) {
		this.spheres[0] = new Sphere();
		this.spheres[0].setMatte(WALL_RAD, [WALL_RAD + 1.0, 40.8, 81.6], [0.0, 0.0, 0.0], [0.75, 0.25, 0.25]);
		this.spheres[1] = new Sphere();
		this.spheres[1].setMatte(WALL_RAD, [-WALL_RAD + 99.0, 40.8, 81.6], [0.0, 0.0, 0.0], [0.25, 0.25, 0.25]);
		this.spheres[2] = new Sphere();
		this.spheres[2].setMatte(WALL_RAD, [50.0, 40.8, WALL_RAD], [0.0, 0.0, 0.0], [0.75, 0.75, 0.75]);
		this.spheres[3] = new Sphere();
		this.spheres[3].setMatte(WALL_RAD, [50.0, 40.8, -WALL_RAD + 270.0], [0.0, 0.0, 0.0], [0.0, 0.0, 0.0]);
		this.spheres[4] = new Sphere();
		this.spheres[4].setMatte(WALL_RAD, [50.0, WALL_RAD, 81.6], [0.0, 0.0, 0.0], [0.75, 0.75, 0.75]);
		this.spheres[5] = new Sphere();
		this.spheres[5].setMatte(WALL_RAD, [50.0, -WALL_RAD + 81.6, 81.6], [0.0, 0.0, 0.0], [0.75, 0.75, 0.75]);
		this.spheres[6] = new Sphere();
		this.spheres[6].setMatte(7.0, [50.0, 81.6 - 15.0, 81.6], [12.0, 12.0, 12.0], [0.0, 0.0, 0.0]);
	}
}

Scene.prototype.getSpheres = function() {
	return this.spheres;
}

Scene.prototype.getSphereCount = function() {
	return this.spheres.length;
}

Scene.prototype.getBuffer = function() {
	var buffer = new Float32Array(this.spheres.length * 15);
	
	for(var i = 0; i < this.spheres.length; i++) {
		buffer[i * 15] = this.spheres[i].rad;
		buffer[i * 15 + 1] = this.spheres[i].p[0];
		buffer[i * 15 + 2] = this.spheres[i].p[1];
		buffer[i * 15 + 3] = this.spheres[i].p[2];
		buffer[i * 15 + 4] = this.spheres[i].e[0];
		buffer[i * 15 + 5] = this.spheres[i].e[1];
		buffer[i * 15 + 6] = this.spheres[i].e[2];
		buffer[i * 15 + 7] = this.spheres[i].material;
		buffer[i * 15 + 8] = this.spheres[i].c[0];
		buffer[i * 15 + 9] = this.spheres[i].c[1];
		buffer[i * 15 + 10] = this.spheres[i].c[2];
		buffer[i * 15 + 11] = 0;
		buffer[i * 15 + 12] = 0;
		buffer[i * 15 + 13] = 0;
		buffer[i * 15 + 14] = 0;
	}

	return buffer;
}
