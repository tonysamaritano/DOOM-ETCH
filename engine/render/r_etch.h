// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef R_ETCH_H
#define R_ETCH_H

#include "motion.h"

// Capture is optional; ordinary gameplay leaves the surface map inactive.
int R_EtchBegin(int width, int height);
void R_EtchColumn(int x, int top, int bottom, int surface);
int R_EtchBuildMotion(MotionPath *path);

#endif
