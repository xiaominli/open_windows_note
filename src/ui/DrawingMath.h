#pragma once
#include <vector>
#include "domain/Models.h"
namespace own {
double pointSegmentDistance(double px, double py, double ax, double ay, double bx, double by);
int strokeHitTest(const std::vector<Stroke>& strokes, int px, int py, double tol);
}
