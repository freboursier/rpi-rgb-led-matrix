#ifndef DMD_SLIDESHOW_H_
#define DMD_SLIDESHOW_H_

#include <vector>
#include <stdio.h>
// #include <Magick++.h>
// #include <magick/image.h>
#include <wand/magick_wand.h>
#include "dmd-slideshow-utils.hh"
#include "led-matrix.h"

using rgb_matrix::RGBMatrix;

#define debug_print(fmt, ...) \
            do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)
				

typedef int64_t tmillis_t;
static const tmillis_t distant_future = (1LL<<40); // that is a while.



void changeBrightnessLevel(RGBMatrix *matrix, int newLevel);
void    goToNextSequence();

#endif