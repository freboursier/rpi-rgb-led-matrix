#include "dmd-slideshow-utils.hh"
#include "dmd-slideshow.h"
#include "FileCollection.hh"

#include <string.h>
#include <stdio.h>



bool    isValidDirent(struct dirent *entry)
{
    if (entry->d_type != DT_REG) {
        return false;
    }
    return isValidImageFilename(entry->d_name);
}

bool    isValidImageFilename(char *filename)
{
    const char *extension = get_filename_ext(filename);
    return (strcmp(extension, "gif") == 0 || strcmp(extension, "jpg"));
}


const char *get_filename_ext(char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

int usage(const char *progname) {
    fprintf(stderr, "usage: %s [options] <image> [option] [<image> ...]\n",
            progname);
    
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
    
    fprintf(stderr,
            "\nSwitch time between files: "
            "-w for static images; -t/-l for animations\n"
            "Animated gifs: If both -l and -t are given, "
            "whatever finishes first determines duration.\n");
    
    fprintf(stderr, "\nThe -w, -t and -l options apply to the following images "
            "until a new instance of one of these options is seen.\n"
            "So you can choose different durations for different images.\n");
    
    return (1);
}

void    setThreadPriority(int priority, uint32_t affinity_mask)
{
    int err;
    if (priority > 0) {
        struct sched_param p;
        p.sched_priority = priority;
        if ((err = pthread_setschedparam(pthread_self(), SCHED_FIFO, &p))) {
            fprintf(stderr, "FYI: Can't set realtime thread priority=%d %s\n",
                    priority, strerror(err));
        }
    }
    
    if (affinity_mask != 0) {
        cpu_set_t cpu_mask;
        CPU_ZERO(&cpu_mask);
        for (int i = 0; i < 32; ++i) {
            if ((affinity_mask & (1<<i)) != 0) {
                CPU_SET(i, &cpu_mask);
            }
        }
        if ((err=pthread_setaffinity_np(pthread_self(), sizeof(cpu_mask), &cpu_mask))) {
            fprintf(stderr, "FYI: Couldn't set affinity 0x%x: %s\n",
                    affinity_mask, strerror(err));
        }
    }
}


void blitzFrameInCanvas(RGBMatrix *matrix, FrameCanvas *offscreen_canvas,
                        Magick::Image &img, unsigned int position,
                        ScreenMode screenMode) {
  if (screenMode == FullScreen) {
    img.scale(Magick::Geometry(matrix->width(), matrix->height()));
  }
  int x_offset = position % 2 == 0 ? 0 : matrix->width() / 2;

  int y_offset = position <= 1 ? 0 : matrix->height() / 2;
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
  setThreadPriority(3, (1 << 2));

  FileCollection *collection = (FileCollection *)inParam;
    fprintf(stderr, "LOAD FILE, got %d files, run until we got %d\n", collection->loadedFiles.size(), collection->visibleImages * 2);
  while (collection->loadedFiles.size() < collection->visibleImages * 2) {
    std::vector<Magick::Image> frames;
    int count = 0;
    int maxTries = 3;
    while (true) {
      int randCount = rand() % collection->filePaths.size();
      const char *imagePath = collection->filePaths[randCount];
      try {
        fprintf(stderr, "Attempt to load >%s<\n", imagePath);
        readImages(&frames, imagePath);

        LoadedFile *loadedFile = new LoadedFile();

        loadedFile->filename = imagePath;
        loadedFile->is_multi_frame = frames.size() > 1;
        loadedFile->frameCount = frames.size();
        loadedFile->currentFrameID = -1;
        loadedFile->nextFrameTime = GetTimeInMillis();
        collection->loadedFiles.push_back(loadedFile);

        break;
      } catch (std::exception &e) {
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
      Magick::coalesceImages(&(collection->loadedFiles.back()->frames),
                             frames.begin(), frames.end());
        frames.clear();
    } else {
      //            &((*loadedFile)[0].frames)->push_back(loadedFile[0]);   //
      //            just a single still image.
    }
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
    if (milli_seconds <= 0) return;
    struct timespec ts;
    ts.tv_sec = milli_seconds / 1000;
    ts.tv_nsec = (milli_seconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}
