#include "silhouette.h"
#include "hatch.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char palette[768], light[256], translation[256];
static Silhouette ring(void)
{
    unsigned char patch[2048] = {32, 0, 32, 0};
    size_t at = 136;
    for (int x = 0; x < 32; ++x) {
        for (int byte = 0; byte < 4; ++byte)
            patch[8 + x * 4 + byte] = (unsigned char)(at >> (8 * byte));
        int posts = x >= 12 && x < 20 ? 2 : 1;
        for (int run = 0; run < posts; ++run) {
            int length = posts == 1 ? 32 : 12;
            patch[at++] = (unsigned char)(run ? 20 : 0);
            patch[at++] = (unsigned char)length; patch[at++] = 0;
            at += (size_t)length; patch[at++] = 0; /* Zero-valued opaque texels. */
        }
        patch[at++] = 255;
    }
    assert(at <= sizeof(patch));
    Silhouette shape = {0}; assert(SilhouetteDecode(&shape, patch, at));
    for (int i = 0; i < 256; ++i) {
        light[i] = translation[i] = (unsigned char)i;
        palette[i * 3] = palette[i * 3 + 1] = palette[i * 3 + 2] = (unsigned char)i;
    }
    return shape;
}
