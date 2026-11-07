#include "rate.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    assert(ETCH_VISUAL_MIN_RATE == 1.0 / 64);
    assert(ETCH_VISUAL_MAX_RATE == 1024);
    assert(EtchVisualRateClamp(0) == 1.0 / 64);
    assert(EtchVisualRateClamp(10000) == 1024);
    assert(EtchVisualRateKey(1, '+') == 2);
    assert(EtchVisualRateKey(1, '=') == 2);
    assert(EtchVisualRateKey(2, '-') == 1);
    assert(EtchVisualRateKey(2, '_') == 1);
    double rate = EtchVisualRateKey(1000, '+');
    assert(rate == 1024 && EtchVisualRateKey(rate, '-') == 512);
    for (int i = 0; i < 100; ++i) rate = EtchVisualRateKey(rate, '-');
    assert(rate == 1.0 / 64);
    assert(EtchVisualRateKey(rate, '-') == rate);
    assert(EtchVisualRateKey(1024, '+') == 1024);
    assert(EtchVisualRateKey(1000, 'x') == 1000);
    char text[32];
    EtchVisualRateText(1000, text, sizeof(text)); assert(!strcmp(text, "1000×"));
    EtchVisualRateText(1, text, sizeof(text)); assert(!strcmp(text, "1×"));
    EtchVisualRateText(.25, text, sizeof(text)); assert(!strcmp(text, "1/4×"));
    EtchVisualRateText(1.5, text, sizeof(text)); assert(!strcmp(text, "1.5×"));
    puts("Shared visual rate: exact bounds and key aliases passed");
    return 0;
}
