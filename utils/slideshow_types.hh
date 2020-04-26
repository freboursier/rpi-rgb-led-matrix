#ifndef SLIDESHOW_TYPE_H
#define SLIDESHOW_TYPE_H
#include "graphics.h"
using rgb_matrix::Color;
typedef int64_t tmillis_t;

typedef enum { FullScreen = 0, Cross = 1, Splash = 2 } ScreenMode;

#define DISPLAY_ENABLED 1
#define DISPLAY_SHOULD_CLEAR 1 << 2
#define DISPLAY_OFF 1 << 3

const Color pinkColor = {.r = 222, .g = 121, .b = 173};
const Color blueColor = {.r = 0, .g = 157, .b = 222};
const Color blackColor = {.r = 0, .g = 0, .b = 0};
const Color whiteColor = {.r = 255, .g = 255, .b = 255};
const Color lightGrey = {.r = 120, .g = 120, .b = 120};
#endif