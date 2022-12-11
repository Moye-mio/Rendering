#pragma once

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <math.h>
#include <algorithm>

#include <DirectXMath.h>
using namespace DirectX;

struct Vec3f {
	Vec3f() : x(0), y(0), z(0) {}
	Vec3f(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
	float x;
	float y;
	float z;
};

struct Vec2i {
	Vec2i(int X, int Y) : x(X), y(Y) {}
	int x;
	int y;
};