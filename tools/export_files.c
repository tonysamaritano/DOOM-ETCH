// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include "export_files.h"

int MotionOutputOpen(MotionOutput *output, const char *prefix, const char *extension)
{
    if (!output || output->stream || !prefix || !extension) return 0;
    MotionOutput next = {0};
    int length = snprintf(next.filename, sizeof(next.filename), "%s.%s", prefix, extension);
    if (length < 0 || (size_t)length >= sizeof(next.filename)) return 0;
    length = snprintf(next.temporary, sizeof(next.temporary), "%s.XXXXXX", next.filename);
    if (length < 0 || (size_t)length >= sizeof(next.temporary)) return 0;
    int fd = mkstemp(next.temporary);
    if (fd < 0) return 0;
    next.stream = fdopen(fd, "w");
    if (!next.stream) { close(fd); unlink(next.temporary); return 0; }
    *output = next;
    return 1;
}

int MotionOutputClose(MotionOutput *output, int success)
{
    if (!output || !output->stream) return 0;
    MotionOutput finished = *output;
    *output = (MotionOutput){0};
    if (fclose(finished.stream)) success = 0;
    if (success && rename(finished.temporary, finished.filename) == 0) return 1;
    unlink(finished.temporary);
    return 0;
}

/* Publish each completed file by rename, so a failed write preserves its predecessor. */
int WriteMotionOutput(const char *prefix, const char *extension, const MotionPath *path)
{
    char filename[PATH_MAX], temporary[PATH_MAX];
    int n = snprintf(filename, sizeof(filename), "%s.%s", prefix, extension);
    if (n < 0 || (size_t)n >= sizeof(filename)) return 0;
    n = snprintf(temporary, sizeof(temporary), "%s.XXXXXX", filename);
    if (n < 0 || (size_t)n >= sizeof(temporary)) return 0;

    int fd = mkstemp(temporary);
    if (fd < 0) return 0;
    FILE *stream = fdopen(fd, "w");
    int success = 0;
    if (stream) {
        /* Assumed feed: 3,600 mm/min for the simulated machine. */
        success = extension[0] == 's' ? MotionWriteSVG(stream, path)
                                     : MotionWriteGCode(stream, path, 3600.0);
        if (fclose(stream)) success = 0;
    } else {
        close(fd);
    }
    if (success && rename(temporary, filename) == 0) return 1;
    unlink(temporary);
    return 0;
}

