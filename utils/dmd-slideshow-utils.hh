
#ifndef DMD_SLIDESHOW_UTILS_H
#define DMD_SLIDESHOW_UTILS_H

#include <dirent.h>
#include <stdint.h>
#include <sys/time.h>
#include "led-matrix.h"
#include <Magick++.h>
#include <magick/image.h>
#include "dmd-slideshow.h"

using rgb_matrix::RGBMatrix;
using rgb_matrix::FrameCanvas;

typedef int64_t tmillis_t;

typedef enum  {
	FullScreen = 0,
	Cross = 1
  } ScreenMode;

bool    isValidDirent(struct dirent *entry);
bool    isValidImageFilename(char *filename);
const char *get_filename_ext(char *filename);
int usage(const char *progname);
void    setThreadPriority(int priority, uint32_t affinity_mask);

void *LoadFile(void *inParam);

tmillis_t GetTimeInMillis();
void SleepMillis(tmillis_t milli_seconds);

void blitzFrameInCanvas(RGBMatrix *matrix, FrameCanvas *offscreen_canvas,
                        Magick::Image &img, unsigned int position,
                        ScreenMode screenMode);
void drawCross(RGBMatrix *matrix, FrameCanvas *offscreen_canvas);

#endif