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
#include <getopt.h>
#include <inttypes.h>

#include <map>
#include <string>
#include <vector>

#include "IRRemote.hh"
#include "LoadedFile.hh"
#include "Sequence.hh"
#include "dmd-slideshow-utils.hh"
#include "dmd-slideshow.hh"
#include "slideshow_types.hh"
#include <wand/magick_wand.h>

#include "largeFont.c"
#include "smallFont.c"
#include "smallestFont.c"

#define DEBUG 0
#define BRIGHTNESS_DISPLAY_DURATION 3
#define FRAME_PER_SECOND 20 // Don't go higher than 20 / 25 or you will drop frames
#define DEFAULT_COLLECTION_DURATION 10
#define DEFAULT_SEQUENCE_DURATION 3600
#define SPLASH_GIF "LOGO_RPI2DMD_Impact_RattenJager.gif"

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
volatile bool show_filename = false;
volatile int display_status = DISPLAY_ENABLED;

static void InterruptHandler(int signo) {
  interrupt_received = true;
}

void changeBrightnessLevel(RGBMatrix *matrix, int newLevel) {
  gl_brightness_timeout = time(0) + BRIGHTNESS_DISPLAY_DURATION;
  matrix->SetBrightness(newLevel);
}

Sequence *goToNextSequence() {
  fprintf(stderr, "Select next sequence, current is %s\n", gl_sequences[gl_sequence_index]->name());
  if (gl_sequences[gl_sequence_index]->transient == true) {
    gl_sequences.erase(gl_sequences.begin() + gl_sequence_index);
    fprintf(stderr, "Trash transient sequence");
  } else {
    int nextSequenceIndex = (gl_sequence_index + 1) % gl_sequences.size();
    if (nextSequenceIndex != gl_sequence_index) {
      gl_sequences[gl_sequence_index]->reset();
      gl_sequence_index = nextSequenceIndex;
      gl_sequences[gl_sequence_index]->setDisplayStartTime(GetTimeInMillis());
    }
  }
  next_sequence_received = false;
  return gl_sequences[gl_sequence_index];
}

void drawBrightness(RGBMatrix *matrix, FrameCanvas *offscreen_canvas, Font *smallFont) {

  int totalWidth = 30;
  int textAreaHeight = 12;
  int borderSize = 2;
  char *level = NULL;

  asprintf(&level, "%d%%", matrix->brightness());

  int originX = matrix->width() - (totalWidth + borderSize);
  int originY = borderSize;

  int height = matrix->height() - borderSize * 2;
  int textWidth = DrawText(offscreen_canvas, *smallFont, originX, originY + smallFont->baseline(), whiteColor, NULL, level, 0);

  FillRectangle(offscreen_canvas, originX, originY, totalWidth, height, blackColor);
  DrawText(offscreen_canvas, *smallFont, (totalWidth - borderSize - textWidth) / 2 + originX + borderSize, originY + smallFont->baseline() + borderSize, whiteColor, NULL, level, 0);

  height = height - textAreaHeight - borderSize;
  FillRectangle(offscreen_canvas, originX + borderSize, originY + textAreaHeight, totalWidth - borderSize * 2, height, lightGrey);

  int enabledHeight = height * matrix->brightness() / 100.0;
  FillRectangle(offscreen_canvas, originX + borderSize, originY + textAreaHeight + (height - enabledHeight), totalWidth - borderSize * 2, enabledHeight, whiteColor);
}

char const *writeFont(const unsigned char fontArray[], const int fontSize) {
  char const *fileTemplate = "/tmp/font.temp";

  int fd = open(fileTemplate, O_CREAT | O_TRUNC | O_WRONLY);
  int ret = write(fd, fontArray, fontSize);
  if (ret != fontSize) {
    fprintf(stderr, "Failed to write font to %s (%d bytes written)\n", fileTemplate, ret);
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
  bool dirtyCanvas = false;

  std::vector<LoadedFile *> *currentImages = NULL;

  Font statusFont;
  Font smallFont;
  Font smallestFont;

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

  while (!interrupt_received) {
    dirtyCanvas = false;
    tmillis_t frame_start = GetTimeInMillis();
    if (display_status == DISPLAY_ENABLED) {
      
  Sequence *seq = gl_sequences[gl_sequence_index];
      if (next_sequence_received || seq->isExpired() ) {
        if (workerThread != 0) {
          pthread_cancel(workerThread);
          workerThread = 0;
        } else {
          seq = goToNextSequence();
          shouldRedrawSequenceName = true;
        }
      }

      
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
            seq->currentCollection()->setDisplayStartTime(GetTimeInMillis() + (seq->transient ? 0 : 3 * 1000));
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
        seq->currentCollection()->setDisplayStartTime(GetTimeInMillis());
      }

      if (seq->currentCollection() == NULL || seq->currentCollection()->displayStartTime() > GetTimeInMillis()) { // bouger eq->currentCollection()->displayStartTime() sur la sequence
        if (shouldRedrawSequenceName == true && seq->transient == false) {
          Color blackColor = {.r = 0, .g = 0, .b = 0};
        offscreen_canvas->Clear();
          int textWidth = DrawText(offscreen_canvas, statusFont, 0, 0 + statusFont.baseline(), blackColor, NULL, seq->name(), 0);

          FillRectangle(offscreen_canvas, 0, matrix->height() / 4, matrix->width(), matrix->height() / 2, blueColor);
          DrawText(offscreen_canvas, statusFont, (matrix->width() - textWidth) / 2 + 1, statusFont.baseline() + (matrix->height() - statusFont.baseline()) / 2 + 1, pinkColor, NULL, seq->name(), 0);

          DrawText(offscreen_canvas, statusFont, (matrix->width() - textWidth) / 2, statusFont.baseline() + (matrix->height() - statusFont.baseline()) / 2, whiteColor, NULL, seq->name(), 0);

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
          if (seq->currentCollection()->displayDuration > 0 && GetTimeInMillis() - seq->currentCollection()->displayStartTime() > (seq->currentCollection()->displayDuration * 1000)) {
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
              int64_t delay_time_us = MagickGetImageDelay((*currentImages)[i]->wand()) * 10000;
              if (delay_time_us == 0) {
                delay_time_us = 50 * 1000; // single image.
              } else if (delay_time_us < 0) {
                delay_time_us = 100 * 1000; // 1/10sec
                fprintf(stderr, "CRAPPY TIMING USE DEFAULT");
              }
              (*currentImages)[i]->nextFrameTime = (*currentImages)[i]->nextFrameTime + delay_time_us / 1000.0;
            }
            blitzFrameInCanvas(matrix, offscreen_canvas, (*currentImages)[i], i, seq->currentCollection()->screenMode, show_filename ? &smallestFont : NULL, &statusFont);
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
    } else {
      if (display_status == DISPLAY_SHOULD_CLEAR) {
        offscreen_canvas->Clear();
        dirtyCanvas = true;
        display_status = DISPLAY_OFF;
      }
    }
    if (dirtyCanvas == true) {
      offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
    }
    tmillis_t ellapsedTime = GetTimeInMillis() - frame_start;
    tmillis_t next_frame = frame_start + (1000.0 / FRAME_PER_SECOND) - ellapsedTime;
    if (next_frame - frame_start < 0) {
      fprintf(stderr, "LOST FRAMES\n");
    }
    SleepMillis(next_frame - frame_start);
  }
}

int main(int argc, char *argv[]) {
  fprintf(stdout, "\033[0;36mStarting Mega DMD\033[0m\n");
  srand(time(0));
  InitializeMagick(*argv);
  pthread_t remoteControlThread = 0;
  std::vector<const char *> filenames = std::vector<const char *>();

  RGBMatrix::Options matrix_options;
  rgb_matrix::RuntimeOptions runtime_opt;
  if (!rgb_matrix::ParseOptionsFromFlags(&argc, &argv, &matrix_options, &runtime_opt)) {
    return usage(argv[0]);
  }
  int displayDuration = DEFAULT_COLLECTION_DURATION;
  int sequenceDuration = DEFAULT_SEQUENCE_DURATION;
  const char *stream_output = NULL;
  int opt;

  static struct option long_options[] = {{"wait", required_argument, 0, 'w'},
                                         {"directory", required_argument, 0, 'd'},
                                         {"cross", required_argument, 0, 'c'},
                                         {"full", required_argument, 0, 'f'},
                                         {"sequence", required_argument, 0, 's'},
                                         {"sequence_duration", required_argument, 0, 'x'},

                                         {"help", no_argument, 0, 0},
                                         {0, 0, 0, 0}};
  int option_index = 0;
  while ((opt = getopt_long(argc, argv, "w:hd:c:f:s:x:", long_options, &option_index)) != -1) {
    switch (opt) {
    case 'x':
      sequenceDuration = atoi(optarg);
      break;
    case 'w':
      displayDuration = atoi(optarg);
      break;
    case 'd':
      getFilenamesFromDirectory(&filenames, optarg);
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
      newSequence->setDisplayTime(sequenceDuration);
      gl_sequences.push_back(newSequence);
    } break;
    case 'h':
    default:
      return usage(argv[0]);
    }
  }

  Sequence *newSequence = new Sequence("splash", true);
  newSequence->setDisplayTime(8);
  FileCollection *newCollection = new FileCollection(Splash, 0, SPLASH_GIF);
  newSequence->collections.push_back(newCollection);
  gl_sequences.insert(gl_sequences.begin(), newSequence);

  fprintf(stderr, "%d valid files\n", filenames.size());

  for (auto &sequence : gl_sequences) {
    sequence->loadCollections(filenames);
    sequence->printContent();
  }

  runtime_opt.do_gpio_init = (stream_output == NULL);
  RGBMatrix *matrix = CreateMatrixFromOptions(matrix_options, runtime_opt);
  if (matrix == NULL) {
    fprintf(stderr, "Fatal: Failed to create matrix\n");
    return 1;
  }

  printf("Matrix size: %dx%d. Hardware gpio mapping: %s\n", matrix->width(), matrix->height(), matrix_options.hardware_mapping);

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
