
#ifndef DMD_SLIDESHOW_UTILS_H
#define DMD_SLIDESHOW_UTILS_H

#include "dmd-slideshow.hh"
#include "graphics.h"
#include "led-matrix.h"
#include <dirent.h>
#include <stdint.h>
#include <sys/time.h>
#include <wand/magick_wand.h>
#include "LoadedFile.hh"

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;
using rgb_matrix::Font;



bool isValidDirent(struct dirent *entry);
bool isValidImageFilename(char *filename);
const char *get_filename_ext(char *filename);
int usage(const char *progname);
void setThreadPriority(int priority, uint32_t affinity_mask);

void *LoadFile(void *inParam);

tmillis_t GetTimeInMillis();
void SleepMillis(tmillis_t milli_seconds);

void blitzFrameInCanvas(RGBMatrix *matrix, FrameCanvas *offscreen_canvas, LoadedFile *loadedFile, unsigned int position, ScreenMode screenMode, Font *smallesFont, Font *largeFont);
void drawCross(RGBMatrix *matrix, FrameCanvas *offscreen_canvas);

void FillRectangle(FrameCanvas *c, int x0, int y0, int width, int height, rgb_matrix::Color color);

int  getFilenamesFromDirectory(std::vector<const char *> *filesames, char *gifDirectory);
#endif