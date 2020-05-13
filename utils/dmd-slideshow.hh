#ifndef DMD_SLIDESHOW_H_
#define DMD_SLIDESHOW_H_

#include <stdio.h>
#include <vector>
#include "dmd-slideshow-utils.hh"
#include "led-matrix.h"
#include <wand/magick_wand.h>

using rgb_matrix::RGBMatrix;


typedef int64_t tmillis_t;
static const tmillis_t distant_future = (1LL << 40); // that is a while.

void changeBrightnessLevel(RGBMatrix *matrix, int newLevel);

#endif