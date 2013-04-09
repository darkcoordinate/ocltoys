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

#include "camera.h"
#include "geom.h"

#define sigma_s .004f
#define sigma_a .0001f
#define sigma_t (sigma_s + sigma_a)

float GetRandom(unsigned int *seed0, unsigned int *seed1) {
	*seed0 = 36969 * ((*seed0) & 65535) + ((*seed0) >> 16);
	*seed1 = 18000 * ((*seed1) & 65535) + ((*seed1) >> 16);

	unsigned int ires = ((*seed0) << 16) + (*seed1);

	/* Convert to float */
	union {
		float f;
		unsigned int ui;
	} res;
	res.ui = (ires & 0x007fffff) | 0x40000000;

	return (res.f - 2.f) / 2.f;
}

float SphereIntersect(
	__global const Sphere *s,
	const Ray *r) { /* returns distance, 0 if nohit */
	Vec op; /* Solve t^2*d.d + 2*t*(o-p).d + (o-p).(o-p)-R^2 = 0 */
	vsub(op, s->p, r->o);

	float b = vdot(op, r->d);
	float det = b * b - vdot(op, op) + s->rad * s->rad;
	if (det < 0.f)
		return 0.f;
	else
		det = sqrt(det);

	float t = b - det;
	if (t >  EPSILON)
		return t;
	else {
		t = b + det;

		if (t >  EPSILON)
			return t;
		else
			return 0.f;
	}
}

int Intersect(
	__global const Sphere *spheres,
	const unsigned int sphereCount,
	const Ray *r,
	float *t,
	unsigned int *id) {
	float inf = (*t) = 1e20f;

	unsigned int i = sphereCount;
	for (; i--;) {
		const float d = SphereIntersect(&spheres[i], r);
		if ((d != 0.f) && (d < *t)) {
			*t = d;
			*id = i;
		}
	}

	return (*t < inf);
}

void CoordinateSystem(const Vec *v1, Vec *v2, Vec *v3) {
	if (fabs(v1->x) > fabs(v1->y)) {
		float invLen = 1.f / sqrt(v1->x * v1->x + v1->z * v1->z);
		v2->x = -v1->z * invLen;
		v2->y = 0.f;
		v2->z = v1->x * invLen;
	} else {
		float invLen = 1.f / sqrt(v1->y * v1->y + v1->z * v1->z);
		v2->x = 0.f;
		v2->y = v1->z * invLen;
		v2->z = -v1->y * invLen;
	}

	vxcross(*v3, *v1, *v2);
}

float SampleSegment(const float epsilon, const float sigma, const float smax) {
	return -log(1.f - epsilon * (1.f - exp(-sigma * smax))) / sigma;
}

void SampleHG(const float g, const float e1, const float e2, Vec *dir) {
	const float s = 1.f - 2.f * e1;
	const float cost = (s + 2.f * g * g * g * (-1.f + e1) * e1 + g * g * s + 2.f * g * (1.f - e1 + e1 * e1)) / ((1.f + g * s)*(1.f + g * s));
	const float sint = sqrt(1.f - cost * cost);

	dir->x = cos(2.f * FLOAT_PI * e2) * sint;
	dir->y = sin(2.f * FLOAT_PI * e2) * sint;
	dir->z = cost;
}

float Scatter(const Ray *currentRay, const float distance, Ray *scatterRay,
		float *scatterDistance, unsigned int *seed0, unsigned int *seed1) {
	*scatterDistance = SampleSegment(GetRandom(seed0, seed1), sigma_s, distance - EPSILON) + EPSILON;

	Vec scatterPoint;
	vsmul(scatterPoint, *scatterDistance, currentRay->d);
	vadd(scatterPoint, currentRay->o, scatterPoint);

	// Sample a direction ~ Henyey-Greenstein's phase function
	Vec dir;
	SampleHG(-.5f, GetRandom(seed0, seed1), GetRandom(seed0, seed1), &dir);

	Vec u, v;
	CoordinateSystem(&currentRay->d, &u, &v);

	Vec scatterDir;
	scatterDir.x = u.x * dir.x + v.x * dir.y + currentRay->d.x * dir.z;
	scatterDir.y = u.y * dir.x + v.y * dir.y + currentRay->d.y * dir.z;
	scatterDir.z = u.z * dir.x + v.z * dir.y + currentRay->d.z * dir.z;

	rinit(*scatterRay, scatterPoint, scatterDir);

	return (1.f - exp(-sigma_s * (distance - EPSILON)));
}

void Radiance(
	__global const Sphere *spheres,
	const unsigned int sphereCount,
	const Ray *startRay,
	unsigned int *seed0, unsigned int *seed1,
	Vec *result) {
	Ray currentRay; rassign(currentRay, *startRay);
	Vec rad; vinit(rad, 0.f, 0.f, 0.f);
	Vec throughput; vinit(throughput, 1.f, 1.f, 1.f);

	unsigned int depth = 0;
	for (;; ++depth) {
		// Removed Russian Roulette in order to improve execution on SIMT
		if (depth > 6) {
			*result = rad;
			return;
		}

		float t; /* distance to intersection */
		unsigned int id = 0; /* id of intersected object */
		const bool hit = Intersect(spheres, sphereCount, &currentRay, &t, &id);

		// Check if there is a scattering event
		Ray scatterRay;
		float scatterDistance;
		const float scatteringProbability = Scatter(&currentRay, hit ? t : 999.f, &scatterRay,
				&scatterDistance, seed0, seed1);

		// Is there the scatter event ?
		if ((scatteringProbability > 0.f) && (GetRandom(seed0, seed1) <= scatteringProbability)) {
			// There is, sample the volume
			rassign(currentRay, scatterRay);

			// Absorption
			const float absorption = exp(-sigma_t * scatterDistance);
			vsmul(throughput, absorption, throughput);
			continue;
		}
			
		if (!hit) {
			*result = rad; /* if miss, return */
			return;
		}
		
		const float absorption = exp(-sigma_t * t);
		vsmul(throughput, absorption, throughput);

		__global const Sphere *obj = &spheres[id]; /* the hit object */

		Vec hitPoint;
		vsmul(hitPoint, t, currentRay.d);
		vadd(hitPoint, currentRay.o, hitPoint);

		Vec normal;
		vsub(normal, hitPoint, obj->p);
		vnorm(normal);

		const float dp = vdot(normal, currentRay.d);
		Vec nl;
		// SIMT optimization
		const float invSignDP = -1.f * sign(dp);
		vsmul(nl, invSignDP, normal);

		/* Add emitted light */
		Vec eCol; vassign(eCol, obj->e);
		if (!viszero(eCol)) {
			vsmul(eCol, fabs(dp), eCol);
			vmul(eCol, throughput, eCol);
			vadd(rad, rad, eCol);

			*result = rad;
			return;
		}

		if (obj->refl == DIFF) { /* Ideal DIFFUSE reflection */
			vmul(throughput, throughput, obj->c);

			/* Diffuse component */

			float r1 = 2.f * FLOAT_PI * GetRandom(seed0, seed1);
			float r2 = GetRandom(seed0, seed1);
			float r2s = sqrt(r2);

			Vec w = nl;
			Vec u, v;
			CoordinateSystem(&w, &u, &v);

			Vec newDir;
			vsmul(u, cos(r1) * r2s, u);
			vsmul(v, sin(r1) * r2s, v);
			vadd(newDir, u, v);
			vsmul(w, sqrt(1 - r2), w);
			vadd(newDir, newDir, w);

			rinit(currentRay, hitPoint, newDir);
			continue;
		} else if (obj->refl == SPEC) { /* Ideal SPECULAR reflection */
			Vec newDir;
			vsmul(newDir,  2.f * vdot(normal, currentRay.d), normal);
			vsub(newDir, currentRay.d, newDir);

			vmul(throughput, throughput, obj->c);

			rinit(currentRay, hitPoint, newDir);
			continue;
		} else {
			Vec newDir;
			vsmul(newDir,  2.f * vdot(normal, currentRay.d), normal);
			vsub(newDir, currentRay.d, newDir);

			Ray reflRay; rinit(reflRay, hitPoint, newDir); /* Ideal dielectric REFRACTION */
			int into = (vdot(normal, nl) > 0); /* Ray from outside going in? */

			float nc = 1.f;
			float nt = 1.5f;
			float nnt = into ? nc / nt : nt / nc;
			float ddn = vdot(currentRay.d, nl);
			float cos2t = 1.f - nnt * nnt * (1.f - ddn * ddn);

			if (cos2t < 0.f)  { /* Total internal reflection */
				vmul(throughput, throughput, obj->c);

				rassign(currentRay, reflRay);
				continue;
			}

			float kk = (into ? 1 : -1) * (ddn * nnt + sqrt(cos2t));
			Vec nkk;
			vsmul(nkk, kk, normal);
			Vec transDir;
			vsmul(transDir, nnt, currentRay.d);
			vsub(transDir, transDir, nkk);
			vnorm(transDir);

			float a = nt - nc;
			float b = nt + nc;
			float R0 = a * a / (b * b);
			float c = 1 - (into ? -ddn : vdot(transDir, normal));

			float Re = R0 + (1 - R0) * c * c * c * c*c;
			float Tr = 1.f - Re;
			float P = .25f + .5f * Re;
			float RP = Re / P;
			float TP = Tr / (1.f - P);

			if (GetRandom(seed0, seed1) < P) { /* R.R. */
				vsmul(throughput, RP, throughput);
				vmul(throughput, throughput, obj->c);

				rassign(currentRay, reflRay);
				continue;
			} else {
				vsmul(throughput, TP, throughput);
				vmul(throughput, throughput, obj->c);

				rinit(currentRay, hitPoint, transDir);
				continue;
			}
		}
	}
}

void GenerateCameraRay(__global const Camera *camera,
		unsigned int *seed0, unsigned int *seed1,
		const int width, const int height, const int x, const int y, Ray *ray) {
	const float invWidth = 1.f / width;
	const float invHeight = 1.f / height;
	const float r1 = GetRandom(seed0, seed1) - .5f;
	const float r2 = GetRandom(seed0, seed1) - .5f;
	const float kcx = (x + r1) * invWidth - .5f;
	const float kcy = (y + r2) * invHeight - .5f;

	Vec rdir;
	vinit(rdir,
			camera->x.x * kcx + camera->y.x * kcy + camera->dir.x,
			camera->x.y * kcx + camera->y.y * kcy + camera->dir.y,
			camera->x.z * kcx + camera->y.z * kcy + camera->dir.z);

	Vec rorig;
	vsmul(rorig, 0.1f, rdir);
	vadd(rorig, rorig, camera->orig)

	vnorm(rdir);
	rinit(*ray, rorig, rdir);
}

__kernel void SmallPTGPU(
    __global Vec *samples, __global unsigned int *seedsInput,
	__global const Camera *camera,
	const unsigned int sphereCount, __global const Sphere *sphere,
	const unsigned int width, const unsigned int height,
	const unsigned int currentSample) {
	const int gid = get_global_id(0);
	// Check if we have to do something
	if (gid >= width * height)
		return;

	const int scrX = gid % width;
	const int scrY = gid / width;

	/* LordCRC: move seed to local store */
	unsigned int seed0 = seedsInput[2 * gid];
	unsigned int seed1 = seedsInput[2 * gid + 1];

	Ray ray;
	GenerateCameraRay(camera, &seed0, &seed1, width, height, scrX, scrY, &ray);

	Vec r;
	Radiance(sphere, sphereCount, &ray, &seed0, &seed1, &r);

	__global Vec *sample = &samples[gid];
	if (currentSample == 0)
		*sample = r;
	else {
		const float k1 = currentSample;
		const float k2 = 1.f / (currentSample + 1.f);
		sample->x = (sample->x * k1  + r.x) * k2;
		sample->y = (sample->y * k1  + r.y) * k2;
		sample->z = (sample->z * k1  + r.z) * k2;
	}

	seedsInput[2 * gid] = seed0;
	seedsInput[2 * gid + 1] = seed1;
}

#define toColor(x) (pow(clamp(x, 0.f, 1.f), 1.f / 2.2f))

__kernel void ToneMapping(
	__global Vec *samples, __global Vec *pixels,
	const unsigned int width, const unsigned int height) {
	const int gid = get_global_id(0);
	// Check if we have to do something
	if (gid >= width * height)
		return;

	__global Vec *sample = &samples[gid];
	__global Vec *pixel = &pixels[gid];
	pixel->x = toColor(sample->x);
	pixel->y = toColor(sample->y);
	pixel->z = toColor(sample->z);
}
