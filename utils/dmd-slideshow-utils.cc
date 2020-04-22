#include "dmd-slideshow-utils.hh"
#include "FileCollection.hh"
#include "Sequence.hh"
#include "dmd-slideshow.hh"
#include "graphics.h"

#include <stdio.h>
#include <string.h>

using rgb_matrix::Color;

bool isValidDirent(struct dirent *entry) {
  if (entry->d_type != DT_REG) {
    return false;
  }
  return isValidImageFilename(entry->d_name);
}

bool isValidImageFilename(char *filename) {
  const char *extension = get_filename_ext(filename);
  return (strcmp(extension, "gif") == 0 || strcmp(extension, "jpg"));
}

const char *get_filename_ext(char *filename) {
  const char *dot = strrchr(filename, '.');
  if (!dot || dot == filename)
    return "";
  return dot + 1;
}

int usage(const char *progname) {
  fprintf(stderr, "usage: %s [options] <image> [option] [<image> ...]\n", progname);

  fprintf(stderr, "Options:\n"
                  "\t-O<streamfile>            : Output to stream-file instead of matrix (Don't need to be root).\n"
                  "\t-C                        : Center images.\n"

                  "\nThese options affect images FOLLOWING them on the command line,\n"
                  "so it is possible to have different options for each image\n"
                  "\t-w<seconds>               : Regular image: "
                  "Wait time in seconds before next image is shown (default: 1.5).\n"
                  "\t-t<seconds>               : "
                  "For animations: stop after this time.\n"
                  "\t-l<loop-count>            : "
                  "For animations: number of loops through a full cycle.\n"
                  "\t                            gif/stream animation with this value. Use -1 to use default value.\n"
                  "\t-V<vsync-multiple>        : For animation (expert): Only do frame vsync-swaps on multiples of refresh (default: 1)\n"

  );

  fprintf(stderr, "\nGeneral LED matrix options:\n");
  rgb_matrix::PrintMatrixFlags(stderr);

  fprintf(stderr, "\nSwitch time between files: "
                  "-w for static images; -t/-l for animations\n"
                  "Animated gifs: If both -l and -t are given, "
                  "whatever finishes first determines duration.\n");

  fprintf(stderr, "\nThe -w, -t and -l options apply to the following images "
                  "until a new instance of one of these options is seen.\n"
                  "So you can choose different durations for different images.\n");

  return (1);
}

void setThreadPriority(int priority, uint32_t affinity_mask) {
  int err;
  if (priority > 0) {
    struct sched_param p;
    p.sched_priority = priority;
    if ((err = pthread_setschedparam(pthread_self(), SCHED_FIFO, &p))) {
      fprintf(stderr, "FYI: Can't set realtime thread priority=%d %s\n", priority, strerror(err));
    }
  }

  if (affinity_mask != 0) {
    cpu_set_t cpu_mask;
    CPU_ZERO(&cpu_mask);
    for (int i = 0; i < 32; ++i) {
      if ((affinity_mask & (1 << i)) != 0) {
        CPU_SET(i, &cpu_mask);
      }
    }
    if ((err = pthread_setaffinity_np(pthread_self(), sizeof(cpu_mask), &cpu_mask))) {
      fprintf(stderr, "FYI: Couldn't set affinity 0x%x: %s\n", affinity_mask, strerror(err));
    }
  }
}

void blitzFrameInCanvas(RGBMatrix *matrix, FrameCanvas *offscreen_canvas, LoadedFile *loadedFile, unsigned int position, ScreenMode screenMode, Font *smallestFont) {
  if (screenMode == FullScreen) {
    MagickScaleImage(loadedFile->wand(), matrix->width(), matrix->height());
  }
  int x_offset = position % 2 == 0 ? 0 : matrix->width() / 2;
  // https://www.imagemagick.org/discourse-server/viewtopic.php?t=31691 for nice speedup
  int y_offset = position <= 1 ? 0 : matrix->height() / 2;

  unsigned long columns = MagickGetImageWidth(loadedFile->wand());
  unsigned long rows = MagickGetImageHeight(loadedFile->wand());
  for (size_t y = 0; y < rows; ++y) {
    for (size_t x = 0; x < columns; ++x) {
      unsigned char pixels[27];
      MagickGetImagePixels(loadedFile->wand(), x, y, 1, 1, "RGBA", CharPixel, pixels);
      offscreen_canvas->SetPixel(x + x_offset, y + y_offset, pixels[0], pixels[1], pixels[2]);
    }
  }
  const char *lastSlash = strrchr(loadedFile->filename(), '/');
  if (smallestFont != NULL && lastSlash != NULL) {
      Color blackColor = {.r = 0, .g = 0, .b = 0};
      Color whiteColor = {.r = 255, .g = 255, .b = 255};
    DrawText(offscreen_canvas, *smallestFont, x_offset, y_offset + smallestFont->baseline(), whiteColor, &blackColor, lastSlash + 1, 0);
  }

}

void drawCross(RGBMatrix *matrix, FrameCanvas *offscreen_canvas) {
  for (int i = 0; i < matrix->width(); i++) {
    offscreen_canvas->SetPixel(i, matrix->height() / 2 - 1, 0, 0, 0);
    offscreen_canvas->SetPixel(i, matrix->height() / 2, 0, 0, 0);
  }
  for (int i = 0; i < matrix->height(); i++) {
    offscreen_canvas->SetPixel(matrix->width() / 2 - 1, i, 0, 0, 0);
    offscreen_canvas->SetPixel(matrix->width() / 2, i, 0, 0, 0);
  }
}

void *LoadFile(void *inParam) {
  Sequence *sequence = (Sequence *)inParam;
  FileCollection *collection = sequence->nextCollection();
  fprintf(stderr, "LOAD FILE, got %d files, run until we got %d\n", collection->loadedFiles.size(), sequence->nextCollectionTargetSize());
  while (collection->loadedFiles.size() < sequence->nextCollectionTargetSize()) {
    MagickWand *tempWand = NewMagickWand();
    int count = 0;
    int maxTries = 3;
    const char *imagePath = NULL;
    while (true) {
      int randCount = rand() % collection->filePaths.size();
      imagePath = collection->filePaths[randCount];
      try {
        if (MagickReadImage(tempWand, imagePath) == MagickFalse) {
          fprintf(stderr, "Failed to read image at %s\n", imagePath);
        }
        break;
      } catch (std::exception &e) {
        fprintf(stderr, "Failed to load file: %s", e.what());
        if (++count == maxTries) {
          fprintf(stderr, "Too many errors when loading file: %s", e.what());
          pthread_exit((void *)1);
        }
      }
    }
    LoadedFile *loadedFile = new LoadedFile(imagePath, MagickCoalesceImages(tempWand));
    collection->loadedFiles.push_back(loadedFile);
    DestroyMagickWand(tempWand);
  }
  pthread_exit((void *)0);
}

/// Time stuff
tmillis_t GetTimeInMillis() {
  struct timeval tp;
  gettimeofday(&tp, NULL);
  return tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

void SleepMillis(tmillis_t milli_seconds) {
  if (milli_seconds <= 0)
    return;
  struct timespec ts;
  ts.tv_sec = milli_seconds / 1000;
  ts.tv_nsec = (milli_seconds % 1000) * 1000000;
  nanosleep(&ts, NULL);
}

void FillRectangle(FrameCanvas *canvas, int x0, int y0, int width, int height, const rgb_matrix::Color color) {
  for (int y = y0; y <= y0 + height; y++) {
    for (int x = x0; x < x0 + width; x++) {
      canvas->SetPixel(x, y, color.r, color.g, color.b);
    }
  }
}

int  getFilenamesFromDirectory(std::vector<const char *> *filenames, char *gifDirectory)
{
   DIR *gifDir = opendir(gifDirectory);
  if (gifDir == NULL) {
    fprintf(stderr, "Cannot open gif directory %s\n", gifDirectory);
    return 0;
  }
  errno = 0;

  while (1) {
    struct dirent *entry = readdir(gifDir);
    if (entry == NULL && errno == 0) {
      break;
    }

    if (isValidDirent(entry)) {
      int mallocSize = sizeof(char) * (strlen(gifDirectory) + strlen(entry->d_name) + 2);
      char *filePath = (char *)malloc(mallocSize);
      sprintf(filePath, "%s/%s", gifDirectory, entry->d_name);
      filenames->push_back(filePath);
    }
    errno = 0;
  }
  return 1;
}