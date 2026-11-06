// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef R_ETCH_H
#define R_ETCH_H

#include "motion.h"

// Projected capture is optional; ordinary gameplay leaves it inactive.
int R_EtchBegin(int width, int height);
void R_EtchFrameBegin(int width, int height);
void R_EtchWallBegin(void);
void R_EtchWallColumn(int tier, int x, double top, double bottom,
                      double topstep, double bottomstep);
void R_EtchClipPlane(int x, double top, double topstep, double bottom, double bottomstep);
void R_EtchWallEnd(void);
int R_EtchBuildMotion(MotionPath *path);
void R_EtchSprites(int enabled);
int R_EtchSpritesActive(void);
int R_EtchSpritePatch(int lump, const void *patch, size_t bytes,
    double origin_x, double origin_y, double scale_x, double scale_y,
    int first, int last, const short *top, const short *bottom);
void R_EtchResetSprites(void);

#endif
