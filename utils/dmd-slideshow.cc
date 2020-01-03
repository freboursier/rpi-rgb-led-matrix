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

#include "led-matrix.h"
#include "pixel-mapper.h"
#include "content-streamer.h"
#include <pthread.h>
#include <dirent.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <regex.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include <Magick++.h>
#include <magick/image.h>

#include "dmd-slideshow.h"
#include    "dmd-slideshow-utils.hh"

//#define    MEGA_VERBOSE
#define	DEBUG	0
#define        IMAGE_DISPLAY_DURATION    5
#define        FRAME_PER_SECOND        30

std::vector<const char *> gl_filenames;
std::vector<FileCollection>	gl_collections;


using rgb_matrix::GPIO;
using rgb_matrix::Canvas;
using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;
using rgb_matrix::StreamReader;


void  *LoadFile(void *inParam)
{
  //   fprintf(stderr, "Start worker thread\n");
  setThreadPriority(3, (1<<2));

  FileCollection	*collection = (FileCollection	*)inParam;
    
  if (collection->screenMode == FullScreen)
    {
      collection->loadedFiles = std::vector<LoadedFile>(1);
    }
  else
    {
      collection->loadedFiles = std::vector<LoadedFile>(4);
    }
	
  for (unsigned int i = 0; i < collection->loadedFiles.size(); i++)
    {
      std::vector<Magick::Image> frames;
      int count = 0;
      int maxTries = 3;
      while (true)
        {
          int    randCount = rand() % collection->filePaths.size();
          //      fprintf(stderr, "Available image count: %d, bet on %d\n", gl_filenames.size(), randCount);
          const char    *imagePath = collection->filePaths[randCount];
          try {
            fprintf(stderr, "Attempt to load >%s<\n", imagePath);
            readImages(&frames, imagePath);

                  (collection->loadedFiles)[i % collection->loadedFiles.size()].filename = imagePath;
      (collection->loadedFiles)[i % collection->loadedFiles.size()].is_multi_frame = frames.size() > 1;
      (collection->loadedFiles)[i % collection->loadedFiles.size()].frameCount = frames.size();
      (collection->loadedFiles)[i % collection->loadedFiles.size()].nextFrameTime = GetTimeInMillis();


            break;
              } catch (std::exception& e) {
            fprintf(stderr, "Failed to load file: %s", e.what());
            if (++count == maxTries)

              {
                fprintf(stderr, "Too many errors when loading file: %s", e.what());
                pthread_exit((void *)1);
              }
          }
        }
      if (frames.size() == 0) {
        fprintf(stderr, "No image found.");
        pthread_exit((void *)1);
      }
      if (frames.size() > 1) {
        Magick::coalesceImages(&((collection->loadedFiles)[i % collection->loadedFiles.size()].frames), frames.begin(), frames.end());
      } else {
        //            &((*loadedFile)[0].frames)->push_back(loadedFile[0]);   // just a single still image.
      }
    }
    
  pthread_exit((void *)0);
}

void    blitzFrameInCanvas(RGBMatrix *matrix, FrameCanvas *offscreen_canvas, Magick::Image &img, unsigned int position, ScreenMode	screenMode)
{
  if (screenMode == FullScreen)
    {
      img.scale(Magick::Geometry(matrix->width(), matrix->height()));
    }
  int    x_offset = position % 2 == 0 ? 0 : matrix->width() / 2;
    
  int    y_offset = position <= 1 ? 0 : matrix->height() / 2;
  for (size_t y = 0; y < img.rows(); ++y) {
    for (size_t x = 0; x < img.columns(); ++x) {
      const Magick::Color &c = img.pixelColor(x, y);
      if (c.alphaQuantum() < 256) {
        offscreen_canvas->SetPixel(x + x_offset, y + y_offset,
                                   ScaleQuantumToChar(c.redQuantum()),
                                   ScaleQuantumToChar(c.greenQuantum()),
                                   ScaleQuantumToChar(c.blueQuantum()));
      }
    }
  }
}

void    drawCross(RGBMatrix *matrix, FrameCanvas *offscreen_canvas)
{
  for (int i = 0; i < matrix->width(); i++)
    {
      offscreen_canvas->SetPixel(i, matrix->height() / 2 - 1,
                                 ScaleQuantumToChar(0),
                                 ScaleQuantumToChar(0),
                                 ScaleQuantumToChar(0));
      offscreen_canvas->SetPixel(i, matrix->height(),
                                 ScaleQuantumToChar(0),
                                 ScaleQuantumToChar(0),
                                 ScaleQuantumToChar(0));
        
    }
  for (int i = 0; i < matrix->height(); i++)
    {
      offscreen_canvas->SetPixel(matrix->width() / 2 - 1, i,
                                 ScaleQuantumToChar(0),
                                 ScaleQuantumToChar(0),
                                 ScaleQuantumToChar(0));
      offscreen_canvas->SetPixel(matrix->width() / 2, i,
                                 ScaleQuantumToChar(0),
                                 ScaleQuantumToChar(0),
                                 ScaleQuantumToChar(0));
        
    }
}

//void        displayLoop(std::vector<const char *> filenames, RGBMatrix *matrix)
void        displayLoop(RGBMatrix *matrix)

{
  FrameCanvas *offscreen_canvas = matrix->CreateFrameCanvas();
  pthread_t workerThread = 0;
  fprintf(stderr, "%d collections available\n", gl_collections.size());
	
  int        frameCount = 0;
  unsigned int		nextCollectionIdx = 0;
  int		currentCollectionIdx = -1;
  std::vector<LoadedFile>	currentImages;
		
  while (!interrupt_received)
    {
      tmillis_t    frame_start = GetTimeInMillis();
        
      if (frameCount % (FRAME_PER_SECOND * IMAGE_DISPLAY_DURATION) == 0 && workerThread == 0)
        {
          int ret = pthread_create(&workerThread, NULL, LoadFile, (void *)(&(gl_collections[nextCollectionIdx])));
          if (ret) {
            printf("Failed to create worker thread\n");
          }
        }
        
      if (workerThread) {
        void    *threadRetval = NULL;
        if (0 == pthread_tryjoin_np(workerThread, &threadRetval)) {
          //          debug_print("\033[0;31mWorker thread has finished\033[0m with value %d\n", (int)threadRetval);
          workerThread = 0;
          currentCollectionIdx = nextCollectionIdx;
          nextCollectionIdx = nextCollectionIdx + 1 %  gl_collections.size();
          currentImages = gl_collections[currentCollectionIdx].loadedFiles;
        }
      }
        
      if (currentCollectionIdx >= 0) {
        bool        shouldChangeDisplay = false;
            
        for (unsigned int i = 0; i < currentImages.size(); i++)
          {
            bool    needFrameChange = GetTimeInMillis() > currentImages[i].nextFrameTime;
            if (needFrameChange) {
              shouldChangeDisplay = true;
              break;
            }
          }

        if (shouldChangeDisplay) {
          for (unsigned int i = 0; i < currentImages.size(); i++)
            {
              bool    needFrameChange = GetTimeInMillis() > currentImages[i].nextFrameTime;
              if (needFrameChange)
                {
                  currentImages[i].currentFrameID = (currentImages[i].currentFrameID + 1) % currentImages[i].frames.size();
                }
                    
              Magick::Image &img = currentImages[i].frames[currentImages[i].currentFrameID];
              if (needFrameChange)
                {
                  int64_t delay_time_us;
                        
                  if (currentImages[i].is_multi_frame) {
                    delay_time_us = img.animationDelay() * 1000; // unit in 1/100s
                  } else {
                    delay_time_us = 5 * 1000;  // single image.
                  }
                  if (delay_time_us <= 0) {
                    delay_time_us = 100 * 1000;  // 1/10sec
                  }
                  currentImages[i].nextFrameTime = GetTimeInMillis() + delay_time_us / 100.0;
                        
                }
              fprintf(stderr, "blitz %s\n", currentImages[i].filename);
              blitzFrameInCanvas(matrix, offscreen_canvas, img, i, gl_collections[currentCollectionIdx].screenMode);
            }
          if (gl_collections[currentCollectionIdx].screenMode == Cross)
            {
              drawCross(matrix, offscreen_canvas);
            }
          offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
        }
      }
        
      tmillis_t    ellapsedTime = GetTimeInMillis() - frame_start;
      tmillis_t    next_frame = frame_start + (1000.0 / FRAME_PER_SECOND) - ellapsedTime;
      SleepMillis(next_frame - frame_start);
        
      frameCount++;
    }
    
}

static bool LoadImageAndScale(const char *filename,
                              int target_width, int target_height,
                              bool fill_width, bool fill_height,
                              std::vector<Magick::Image> *result,
                              std::string *err_msg) {
  std::vector<Magick::Image> frames;
  try {
    readImages(&frames, filename);
  } catch (std::exception& e) {
    if (e.what()) *err_msg = e.what();
    return false;
  }
  if (frames.size() == 0) {
    fprintf(stderr, "No image found.");
    return false;
  }
    
  // Put together the animation from single frames. GIFs can have nasty
  // disposal modes, but they are handled nicely by coalesceImages()
  if (frames.size() > 1) {
    Magick::coalesceImages(result, frames.begin(), frames.end());
  } else {
    result->push_back(frames[0]);   // just a single still image.
  }

  return true;
}


int main(int argc, char *argv[]) {
  srand(time(0));
  Magick::InitializeMagick(*argv);
  RGBMatrix::Options matrix_options;
  rgb_matrix::RuntimeOptions runtime_opt;
  if (!rgb_matrix::ParseOptionsFromFlags(&argc, &argv,
                                         &matrix_options, &runtime_opt)) {
    return usage(argv[0]);
  }
  int	displayDuration = 10;
    
    
  const char *stream_output = NULL;
  char *gifDirectory = NULL;
  int opt;
  while ((opt = getopt(argc, argv, "w:t:l:c:P:hO:d:c:f:")) != -1) {
    switch (opt) {
    case 'w':
      displayDuration = atoi(optarg);
      break;
    case 'd':
      gifDirectory = optarg;
      break;
    case 'c':
      {
        FileCollection	newCollection;
        newCollection.screenMode = Cross;
        newCollection.displayDuration = displayDuration;
        newCollection.regex = optarg;
        gl_collections.push_back(newCollection);
      }
      break;
    case 'f':
      {
        FileCollection	newCollection;
        newCollection.screenMode = FullScreen;
        newCollection.displayDuration = displayDuration;
        newCollection.regex = optarg;
        gl_collections.push_back(newCollection);
      }
      break;
    case 'P':
      matrix_options.parallel = atoi(optarg);
      break;
    case 'h':
    default:
      return usage(argv[0]);
    }
  }
    
    
  DIR    *gifDir = opendir(gifDirectory);
  if (gifDir == NULL)
    {
      fprintf(stderr, "Cannot open gif directory %s\n", gifDirectory);
      return 1;
    }
  errno = 0;
    
  while (1)
    {
      struct dirent *entry = readdir(gifDir);
      if (entry == NULL && errno == 0)
        {
          break;
        }
        
      if (isValidDirent(entry)) {
        int    mallocSize = sizeof(char) * (strlen(gifDirectory) + strlen(entry->d_name) + 2);
        char *filePath = ( char *)malloc(mallocSize);
        sprintf(filePath, "%s/%s", gifDirectory, entry->d_name);
        gl_filenames.push_back(filePath);
      }
      errno = 0;
    }
    
  fprintf(stderr, "%d valid files\n", gl_filenames.size());
#ifdef    MEGA_VERBOSE
  for (unsigned int i = 0; i < gl_filenames.size(); i++)
    {
      fprintf(stderr, ">%s<\n", gl_filenames[i]);
    }
#endif

	
  for (unsigned int i = 0; i < gl_collections.size(); i++)
    {	
      fprintf(stderr, "Fill collection %s\n", gl_collections[i].regex);
		
      regex_t regex;

      int reti = regcomp(&regex, gl_collections[i].regex, REG_ICASE);
      if (reti) {
        fprintf(stderr, "Could not compile regex\n");
      }
		
      for (unsigned int j = 0; j < gl_filenames.size(); j++)
        {
          reti = regexec(&regex, gl_filenames[j], 0, NULL, 0);
          if (!reti) {
#ifdef    MEGA_VERBOSE
            fprintf(stderr,"Match %s => %s\n", gl_collections[i].regex, gl_filenames[j]);
#endif
            gl_collections[i].filePaths.push_back(gl_filenames[j]);
			   
          }
        }
      fprintf(stderr, "\t%d pictures\n", gl_collections[i].filePaths.size());
      regfree(&regex);
		
    }
	
  for (unsigned int z = 0; z < gl_collections.size(); z++)
    {
      fprintf(stderr, "Collection %d %s => %d images\n", z,  gl_collections[z].regex, gl_collections[z].filePaths.size());
    }

  runtime_opt.do_gpio_init = (stream_output == NULL);
  RGBMatrix *matrix = CreateMatrixFromOptions(matrix_options, runtime_opt);
  if (matrix == NULL)
    {
      fprintf(stderr, "Fatal: Failed to create matrix\n");
      return 1;
    }
    
  printf("Size: %dx%d. Hardware gpio mapping: %s\n",
         matrix->width(), matrix->height(), matrix_options.hardware_mapping);
    
    
  fprintf(stderr, "%d available images\n", gl_filenames.size());
    
  signal(SIGTERM, InterruptHandler);
  signal(SIGINT, InterruptHandler);
    
  displayLoop(matrix);

  matrix->Clear();
  delete matrix;
    
  return 0;
}

