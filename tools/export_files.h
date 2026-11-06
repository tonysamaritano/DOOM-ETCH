// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef DOOM_EXPORT_FILES_H
#define DOOM_EXPORT_FILES_H
#include <limits.h>
#include "motion.h"

/* Initialize to zero. Close publishes by rename only when success is true. */
typedef struct {
    FILE *stream;
    char filename[PATH_MAX], temporary[PATH_MAX];
} MotionOutput;
int MotionOutputOpen(MotionOutput *output, const char *prefix, const char *extension);
int MotionOutputClose(MotionOutput *output, int success);
int WriteMotionOutput(const char *prefix, const char *extension, const MotionPath *path);
#endif
