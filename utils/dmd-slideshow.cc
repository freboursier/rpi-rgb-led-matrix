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

#include "IRRemote.hh"
#include "dmd-slideshow-utils.hh"
#include "dmd-slideshow.hh"
#include <wand/magick_wand.h>

//#define    MEGA_VERBOSE

#define DEBUG 0
#define IMAGE_DISPLAY_DURATION 5
#define FRAME_PER_SECOND 30
#define INFO_MESSAGE_LENGTH 30

std::vector<const char *> gl_filenames;
std::vector<Sequence *> gl_sequences;
int gl_sequence_index = 0;

using rgb_matrix::Canvas;
using rgb_matrix::Font;
using rgb_matrix::FrameCanvas;
using rgb_matrix::GPIO;
using rgb_matrix::RGBMatrix;
using rgb_matrix::StreamReader;

char gl_infoMessage[INFO_MESSAGE_LENGTH];
time_t gl_infotimeout = 0;

volatile bool interrupt_received = false;
volatile bool next_sequence_received = false;
static void InterruptHandler(int signo) { interrupt_received = true; }

void scheduleInfoMessage() { gl_infotimeout = time(0) + 3; }

void goToNextSequence() {
  fprintf(stderr, "Select next sequence, current is %s\n", gl_sequences[gl_sequence_index]->name);
  int nextSequenceIndex = (gl_sequence_index + 1) % gl_sequences.size();
  if (nextSequenceIndex != gl_sequence_index) {
    gl_sequences[gl_sequence_index]->reset();
    gl_sequence_index = nextSequenceIndex;
  }
  next_sequence_received = false;
}

void displayLoop(RGBMatrix *matrix) {
  FrameCanvas *offscreen_canvas = matrix->CreateFrameCanvas();
  pthread_t workerThread = 0;

  bool shouldChangeCollection = false;
  std::vector<LoadedFile *> *currentImages;

  Font statusFont;
  if (!statusFont.LoadFont("../fonts/10x20.bdf")) {
    fprintf(stderr, "Couldn't load font \n");
    exit(1);
  }

  while (!interrupt_received) {
    if (next_sequence_received) {
      if (workerThread != 0) {
        pthread_cancel(workerThread);
        workerThread = 0;
      } else {
        goToNextSequence();
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
          seq->currentCollection()->displayStartTime = GetTimeInMillis();
        } else if (threadRetval == PTHREAD_CANCELED) {
          fprintf(stderr, "## Thread has been canceled, go to next sequence");
        }
      }
    }
    if (shouldChangeCollection == true && seq->nextCollectionIsReady() && workerThread == 0) {
      seq->forwardCollection();
      currentImages = &(seq->currentCollection()->loadedFiles);
      seq->currentCollection()->displayStartTime = GetTimeInMillis();
    }

    if (seq->currentCollection() == NULL) {
      // rgb_matrix::Color red = {.r = 255, .g = 0, .b = 0};

      // matrix->Clear();
      // DrawText(offscreen_canvas, statusFont, 0, 0 + statusFont.baseline(), {.r = 255, .g = 255, .b = 255}, &red, seq->name, 0);
      // offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
      rgb_matrix::Color backgroundColor = {.r = 0, .g = 87, .b = 146};
      int textWidth = DrawText(offscreen_canvas, statusFont, 0, 0 + statusFont.baseline(), backgroundColor, &backgroundColor, seq->name, 0);
      // matrix->Clear();

      matrix->Fill(backgroundColor.r, backgroundColor.g, backgroundColor.b);
     // fprintf(stderr, "Draw %s\n", seq->name);
      DrawText(offscreen_canvas, statusFont, (matrix->width() - textWidth) / 2, 0 + statusFont.baseline(), {.r = 253, .g = 95, .b = 0}, &backgroundColor, seq->name, 0);
      // offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
    } else {
      shouldChangeCollection = false;
      bool shouldChangeDisplay = false;

      for (unsigned short i = 0; i < seq->currentCollection()->visibleImages; i++) {
        bool needFrameChange = GetTimeInMillis() > (*currentImages)[i]->nextFrameTime;
        if (needFrameChange) {
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
            if (MagickNextImage((*currentImages)[i]->wand) == MagickFalse) {
              MagickResetIterator((*currentImages)[i]->wand);
              if (seq->currentCollection()->displayDuration == 0) {
                fprintf(stderr, "\033[0;34mCurrent animation is FINISHED, should move to next collection \033\n"); // simplifier: faire ce code après les affichage
                                                                                                                   // pour se débarasser du shouldChangeCollection et
                                                                                                                   // forweard() après les blitz a l'écran
                shouldChangeCollection = true;
              }
            }
          }

          if (needFrameChange) {
            int64_t delay_time_us = MagickGetImageDelay((*currentImages)[i]->wand);
            if (delay_time_us == 0) {
              delay_time_us = 50 * 1000; // single image.
            } else if (delay_time_us < 0) {
              delay_time_us = 100 * 1000; // 1/10sec
              fprintf(stderr, "CRAPPY TIMING USE DEFAULT");
            }

            (*currentImages)[i]->nextFrameTime = GetTimeInMillis() + delay_time_us / 100.0;
          }

          blitzFrameInCanvas(matrix, offscreen_canvas, (*currentImages)[i]->wand, i, seq->currentCollection()->screenMode);
        }
        if (seq->currentCollection()->screenMode == Cross) {
          drawCross(matrix, offscreen_canvas);
        }

        if (time(0) < gl_infotimeout) {
          rgb_matrix::Color blackColor = {.r = 0, .g = 0, .b = 0};
          DrawText(offscreen_canvas, statusFont, 0, 0 + statusFont.baseline(), {.r = 255, .g = 255, .b = 255}, &blackColor, gl_infoMessage, 0);
        }

        //  offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
      }
    }
    offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
    tmillis_t ellapsedTime = GetTimeInMillis() - frame_start;
    tmillis_t next_frame = frame_start + (1000.0 / FRAME_PER_SECOND) - ellapsedTime;
    SleepMillis(next_frame - frame_start);
  }
}

int main(int argc, char *argv[]) {
  srand(time(0));
  InitializeMagick(*argv);
  pthread_t remoteControlThread = 0;

  RGBMatrix::Options matrix_options;
  rgb_matrix::RuntimeOptions runtime_opt;
  if (!rgb_matrix::ParseOptionsFromFlags(&argc, &argv, &matrix_options, &runtime_opt)) {
    return usage(argv[0]);
  }
  int displayDuration = 10;

  const char *stream_output = NULL;
  char *gifDirectory = NULL;
  int opt;
  while ((opt = getopt(argc, argv, "w:t:l:c:P:hO:d:c:f:s:")) != -1) {
    switch (opt) {
    case 'w':
      displayDuration = atoi(optarg);
      break;
    case 'd':
      gifDirectory = optarg;
      break;
    case 'c': {
      FileCollection *newCollection = new FileCollection();
      newCollection->screenMode = Cross;
      newCollection->displayDuration = displayDuration;
      newCollection->regex = optarg;
      newCollection->loadedFiles = std::vector<LoadedFile *>();
      newCollection->visibleImages = 4;
      gl_sequences.back()->collections.push_back(newCollection);
    } break;
    case 'f': {
      FileCollection *newCollection = new FileCollection();
      newCollection->screenMode = FullScreen;
      newCollection->displayDuration = displayDuration;
      newCollection->regex = optarg;
      newCollection->loadedFiles = std::vector<LoadedFile *>();
      newCollection->visibleImages = 1;
      gl_sequences.back()->collections.push_back(newCollection);
    } break;
    case 'P':
      matrix_options.parallel = atoi(optarg);
      break;
    case 's': {
      Sequence *newSequence = new Sequence();
      newSequence->name = optarg;
      gl_sequences.push_back(newSequence);
    } break;
    case 'h':
    default:
      return usage(argv[0]);
    }
  }

  DIR *gifDir = opendir(gifDirectory);
  if (gifDir == NULL) {
    fprintf(stderr, "Cannot open gif directory %s\n", gifDirectory);
    return 1;
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
      gl_filenames.push_back(filePath);
    }
    errno = 0;
  }

  fprintf(stderr, "%d valid files\n", gl_filenames.size());
#ifdef MEGA_VERBOSE
  for (unsigned int i = 0; i < gl_filenames.size(); i++) {
    fprintf(stderr, ">%s<\n", gl_filenames[i]);
  }
#endif

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

  fprintf(stderr, "%d available images\n", gl_filenames.size());

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
