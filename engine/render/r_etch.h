// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef R_ETCH_H
#define R_ETCH_H

// Capture is optional; ordinary gameplay leaves the writer closed.
int R_EtchBegin(const char *path, int width, int height);
void R_EtchColumn(int x, int top, int bottom, int surface);
int R_EtchEnd(void);

#endif
