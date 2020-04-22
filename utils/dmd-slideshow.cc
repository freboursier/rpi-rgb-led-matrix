// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Copyright (C) 2015 Henner Zeller <h.zeller@acm.org>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation version 2.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://gnu.org/licenses/gpl-2.0.txt>

// To use this image viewer, first get image-magick development files
// $ sudo apt-get install libgraphicsmagick++-dev libwebp-dev
//
// Then compile with
// $ make dmd-slideshow

#include "content-streamer.h"
#include "graphics.h"
#include "led-matrix.h"
#include "pixel-mapper.h"

#include <dirent.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>

#include <map>
#include <string>
#include <vector>

#include "Sequence.hh"
#include "LoadedFile.hh"
#include "IRRemote.hh"
#include "dmd-slideshow-utils.hh"
#include "dmd-slideshow.hh"
#include <wand/magick_wand.h>

#include "largeFont.c"
#include "smallFont.c"
#include "smallestFont.c"
//#define    MEGA_VERBOSE

#define DEBUG 0
#define BRIGHTNESS_DISPLAY_DURATION 3
#define FRAME_PER_SECOND 30
#define DEFAULT_DISPLAY_DURATION 10

std::vector<Sequence *> gl_sequences;
int gl_sequence_index = 0;

using rgb_matrix::Canvas;
using rgb_matrix::Color;
using rgb_matrix::Font;
using rgb_matrix::FrameCanvas;
using rgb_matrix::GPIO;
using rgb_matrix::RGBMatrix;
using rgb_matrix::StreamReader;

time_t gl_brightness_timeout = 0;

volatile bool interrupt_received = false;
volatile bool next_sequence_received = false;
static void InterruptHandler(int signo) {
  interrupt_received = true;
}

void changeBrightnessLevel(RGBMatrix *matrix, int newLevel) {
  gl_brightness_timeout = time(0) + BRIGHTNESS_DISPLAY_DURATION;
  matrix->SetBrightness(newLevel);
}

void goToNextSequence() {
  fprintf(stderr, "Select next sequence, current is %s\n", gl_sequences[gl_sequence_index]->name());
  int nextSequenceIndex = (gl_sequence_index + 1) % gl_sequences.size();
  if (nextSequenceIndex != gl_sequence_index) {
    gl_sequences[gl_sequence_index]->reset();
    gl_sequence_index = nextSequenceIndex;
  }
  next_sequence_received = false;
}

void drawBrightness(RGBMatrix *matrix, FrameCanvas *offscreen_canvas, Font *smallFont) {

  int totalWidth = 30;
  int textAreaHeight = 12;
  int width = totalWidth;
  int borderSize = 2;
  char *level = NULL;

  asprintf(&level, "%d%%", matrix->brightness());

  int originX = matrix->width() - (width + borderSize);
  int originY = borderSize;

  Color blackColor = {.r = 0, .g = 0, .b = 0};
  Color whiteColor = {.r = 255, .g = 255, .b = 255};
  Color lightGrey = {.r = 120, .g = 120, .b = 120};

  int height = matrix->height() - borderSize * 2;
  int textWidth = DrawText(offscreen_canvas, *smallFont, originX, originY + smallFont->baseline(), whiteColor, NULL, level, 0);

  FillRectangle(offscreen_canvas, originX, originY, width, height, blackColor);
  DrawText(offscreen_canvas, *smallFont, (totalWidth - borderSize - textWidth) / 2 + originX + borderSize, originY + smallFont->baseline() + borderSize, whiteColor, NULL, level, 0);

  height = height - textAreaHeight - borderSize;
  FillRectangle(offscreen_canvas, originX + borderSize, originY + textAreaHeight, width - borderSize * 2, height, lightGrey);

  int enabledHeight = height * matrix->brightness() / 100.0;
  FillRectangle(offscreen_canvas, originX + borderSize, originY + textAreaHeight + (height - enabledHeight), width - borderSize * 2, enabledHeight, whiteColor);
}

char const *writeFont(const unsigned char fontArray[], const int fontSize) {
  char const *fileTemplate = "/tmp/font.temp";

  int fd = open(fileTemplate, O_CREAT | O_TRUNC | O_WRONLY);
  int ret = write(fd, fontArray, fontSize);
  if (ret != fontSize) {
    fprintf(stderr, "Failed to write front to %s (%d bytes written)\n", fileTemplate, ret);
    exit(1);
  }
  close(fd);

  return fileTemplate;
}

void displayLoop(RGBMatrix *matrix) {
  FrameCanvas *offscreen_canvas = matrix->CreateFrameCanvas();
  pthread_t workerThread = 0;

  bool shouldChangeCollection = false;
  bool shouldRedrawSequenceName = true;
  std::vector<LoadedFile *> *currentImages;

  Font statusFont;
  Font smallFont;
  Font  smallestFont;

  char const *fontFilename = writeFont(smallFontHex, smallFontHex_size);
  if (!smallFont.LoadFont(fontFilename)) {
    fprintf(stderr, "Couldn't load smallFont \n");
    exit(1);
  }
  fontFilename = writeFont(largeFontHex, largeFontHex_size);
  if (!statusFont.LoadFont(fontFilename)) {
    fprintf(stderr, "Couldn't load largefont \n");
    exit(1);
  }
  fontFilename = writeFont(smallestFontHex, smallestFontHex_size);
  if (!smallestFont.LoadFont(fontFilename)) {
    fprintf(stderr, "Couldn't load smallest font \n");
    exit(1);
  }
  bool dirtyCanvas = false;

  while (!interrupt_received) {
    dirtyCanvas = false;
    if (next_sequence_received) {
      if (workerThread != 0) {
        pthread_cancel(workerThread);
        workerThread = 0;
      } else {
        goToNextSequence();
        shouldRedrawSequenceName = true;
      }
    }
    tmillis_t frame_start = GetTimeInMillis();
    Sequence *seq = gl_sequences[gl_sequence_index];
    if (workerThread == 0 && false == seq->nextCollectionIsReady()) {
      fprintf(stderr, " => %d loaded files in collection %s, need %d\n", seq->nextCollection()->loadedFiles.size(), seq->nextCollection()->regex, seq->nextCollection()->visibleImages);

      int ret = pthread_create(&workerThread, NULL, LoadFile, (void *)seq);
      if (ret) {
        printf("Failed to create worker thread : %d\n", ret);
      }
    }

    if (workerThread) {
      void *threadRetval = NULL;
      if (0 == pthread_tryjoin_np(workerThread, &threadRetval)) {
        fprintf(stderr, "\033[0;31mWorker thread has finished\033[0m with value %d\n", (int)threadRetval);
        workerThread = 0;
        if (seq->currentCollection() == NULL && (int)threadRetval == 0) {
          seq->forwardCollection();
          currentImages = &(seq->currentCollection()->loadedFiles);
          seq->currentCollection()->displayStartTime = GetTimeInMillis() + 3 * 1000;
        } else if (threadRetval == PTHREAD_CANCELED) {
          fprintf(stderr, "## Thread has been canceled, go to next sequence");
          goToNextSequence();
          shouldRedrawSequenceName = true;
        }
      }
    }
    if (shouldChangeCollection == true && seq->nextCollectionIsReady() && workerThread == 0) {
      seq->forwardCollection();
      currentImages = &(seq->currentCollection()->loadedFiles);
      seq->currentCollection()->displayStartTime = GetTimeInMillis();
    }

    if (seq->currentCollection() == NULL || seq->currentCollection()->displayStartTime > GetTimeInMillis()) {
      if (shouldRedrawSequenceName == true) {
        Color backgroundColor = {.r = 241, .g = 172, .b = 71};
        Color blackColor = {.r = 0, .g = 0, .b = 0};

        int textWidth = DrawText(offscreen_canvas, statusFont, 0, 0 + statusFont.baseline(), blackColor, NULL, seq->name(), 0);

        FillRectangle(offscreen_canvas, 0, 0, matrix->width(), matrix->height(), backgroundColor);
        DrawText(offscreen_canvas, statusFont, (matrix->width() - textWidth) / 2 + 1, statusFont.baseline() + (matrix->height() - statusFont.baseline()) / 2 + 1, blackColor, NULL, seq->name(), 0);

        DrawText(offscreen_canvas, statusFont, (matrix->width() - textWidth) / 2, statusFont.baseline() + (matrix->height() - statusFont.baseline()) / 2, {.r = 255, .g = 255, .b = 255}, NULL,
                 seq->name(), 0);

        shouldRedrawSequenceName = false;
        dirtyCanvas = true;
      }
    } else {
      shouldRedrawSequenceName = false;
      shouldChangeCollection = false;
      bool shouldChangeDisplay = false;

      for (unsigned short i = 0; i < seq->currentCollection()->visibleImages; i++) {
        if (true == (GetTimeInMillis() > (*currentImages)[i]->nextFrameTime)) {
          shouldChangeDisplay = true;
          break;
        }
      }

      if (shouldChangeDisplay) {
        if (seq->currentCollection()->displayDuration > 0 && GetTimeInMillis() - seq->currentCollection()->displayStartTime > (seq->currentCollection()->displayDuration * 1000)) {
          fprintf(stderr, "\033[0;34mTime is up for current collection, should move to next \033\n");
          shouldChangeCollection = true;
        }

        for (unsigned short i = 0; i < seq->currentCollection()->visibleImages; i++) {
          bool needFrameChange = GetTimeInMillis() > (*currentImages)[i]->nextFrameTime;

          if (needFrameChange) {
            if (MagickNextImage((*currentImages)[i]->wand()) == MagickFalse) {
              MagickResetIterator((*currentImages)[i]->wand());
              if (seq->currentCollection()->displayDuration == 0) {
                fprintf(stderr, "\033[0;34mCurrent animation is FINISHED, should move to next collection \033\n");
                shouldChangeCollection = true;
              }
            }
          }

          if (needFrameChange) {
            int64_t delay_time_us = MagickGetImageDelay((*currentImages)[i]->wand());
            if (delay_time_us == 0) {
              delay_time_us = 50 * 1000; // single image.
            } else if (delay_time_us < 0) {
              delay_time_us = 100 * 1000; // 1/10sec
              fprintf(stderr, "CRAPPY TIMING USE DEFAULT");
            }
            (*currentImages)[i]->nextFrameTime = GetTimeInMillis() + delay_time_us / 100.0;
          }
          blitzFrameInCanvas(matrix, offscreen_canvas, (*currentImages)[i], i, seq->currentCollection()->screenMode, &smallestFont);
        }
        if (seq->currentCollection()->screenMode == Cross) {
          drawCross(matrix, offscreen_canvas);
        }
        dirtyCanvas = true;
      }
    }
    if (time(0) < gl_brightness_timeout) {
      drawBrightness(matrix, offscreen_canvas, &smallFont);
      dirtyCanvas = true;
      shouldRedrawSequenceName = true;
    }
    fprintf(stderr, "/");
    if (dirtyCanvas == true) {

      offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
    }
    tmillis_t ellapsedTime = GetTimeInMillis() - frame_start;
    tmillis_t next_frame = frame_start + (1000.0 / FRAME_PER_SECOND) - ellapsedTime;
    SleepMillis(next_frame - frame_start);
  }
}

int main(int argc, char *argv[]) {
  srand(time(0));
  InitializeMagick(*argv);
  pthread_t remoteControlThread = 0;
  std::vector<const char *> gl_filenames = std::vector<const char *>();

  RGBMatrix::Options matrix_options;
  rgb_matrix::RuntimeOptions runtime_opt;
  if (!rgb_matrix::ParseOptionsFromFlags(&argc, &argv, &matrix_options, &runtime_opt)) {
    return usage(argv[0]);
  }
  int displayDuration = DEFAULT_DISPLAY_DURATION;

  const char *stream_output = NULL;
  int opt;
  while ((opt = getopt(argc, argv, "w:c:hd:c:f:s:")) != -1) {
    switch (opt) {
    case 'w':
      displayDuration = atoi(optarg);
      break;
    case 'd':
      getFilenamesFromDirectory(&gl_filenames, optarg);
      break;
    case 'c': {
      FileCollection *newCollection = new FileCollection(Cross, displayDuration, optarg);
      gl_sequences.back()->collections.push_back(newCollection);
    } break;
    case 'f': {
      FileCollection *newCollection = new FileCollection(FullScreen, displayDuration, optarg);
      gl_sequences.back()->collections.push_back(newCollection);
    } break;
    case 's': {
      Sequence *newSequence = new Sequence(optarg);
      gl_sequences.push_back(newSequence);
    } break;
    case 'h':
    default:
      return usage(argv[0]);
    }
  }

  fprintf(stderr, "%d valid files\n", gl_filenames.size());

  for (auto &sequence : gl_sequences) {
    sequence->loadCollections(gl_filenames);
    sequence->printContent();
  }

  runtime_opt.do_gpio_init = (stream_output == NULL);
  RGBMatrix *matrix = CreateMatrixFromOptions(matrix_options, runtime_opt);
  if (matrix == NULL) {
    fprintf(stderr, "Fatal: Failed to create matrix\n");
    return 1;
  }

  printf("Size: %dx%d. Hardware gpio mapping: %s\n", matrix->width(), matrix->height(), matrix_options.hardware_mapping);

  signal(SIGTERM, InterruptHandler);
  signal(SIGINT, InterruptHandler);

  int ret = pthread_create(&remoteControlThread, NULL, MonitorIRRemote, (void *)matrix);
  if (ret) {
    printf("Failed to create Remote control thread\n");
  }
  gl_sequence_index = 0;
  displayLoop(matrix);

  matrix->Clear();
  delete matrix;

  return 0;
}
