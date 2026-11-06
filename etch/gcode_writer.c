// SPDX-License-Identifier: GPL-2.0-or-later
#include "motion.h"
#include <math.h>
#include <string.h>

// ASSUMPTION: the device advances in 0.05 mm increments on each axis.
static const double step_mm = 0.05;

/* Fixed decimal notation is accepted by GRBL; exponent notation is not.
 * Seventeen significant digits retain the chosen finite double coordinate. */
static int number(char text[128], double value)
{
    if (!isfinite(value) || value < 0) return 0;
    int decimals = value > 0 ? 16 - (int)floor(log10(value)) : 0;
    if (decimals < 0) decimals = 0;
    int length = snprintf(text, 128, "%.*f", decimals, value + 0.0);
    if (length <= 0 || length >= 128) return 0;
    if (strchr(text, '.')) {
        while (length && text[length - 1] == '0') text[--length] = 0;
        if (length && text[length - 1] == '.') text[--length] = 0;
    }
    return length > 0;
}
