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
#include "graphics.h"

#include <pthread.h>
#include <dirent.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <linux/input.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "Sequence.hh"

#include "dmd-slideshow.h"
#include "dmd-slideshow-utils.hh"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) < (b)) ? (b) : (a))
//#define    MEGA_VERBOSE


#define BRIGHTNESS_INCREMENT  5
#define DEBUG 0
#define IMAGE_DISPLAY_DURATION 5
#define FRAME_PER_SECOND 30
#define INFO_MESSAGE_LENGTH   30

std::vector<const char *> gl_filenames;
std::vector<Sequence *> gl_sequences;

using rgb_matrix::Canvas;
using rgb_matrix::FrameCanvas;
using rgb_matrix::GPIO;
using rgb_matrix::RGBMatrix;
using rgb_matrix::StreamReader;
using rgb_matrix::Font;

RGBMatrix *matrix;
char  gl_infoMessage[INFO_MESSAGE_LENGTH];
time_t  gl_infotimeout = 0;

volatile bool interrupt_received = false;
static void InterruptHandler(int signo) {
    interrupt_received = true;
}


void  scheduleInfoMessage()
{
    gl_infotimeout = time(0) + 3;
}

void *MonitorIRRemote(void *inParam)
{
    int fd, rd;
    unsigned int i;
    struct input_event ev[64];
    
    const char *inputDevice = "/dev/input/event0";
    if ((fd = open(inputDevice, O_RDONLY)) < 0)
    {
        perror("Failed to open IR event source");
        return (void *)1;
    }
    
    while (1)
    {
        rd = read(fd, ev, sizeof(struct input_event) * 64);
        
        if (rd < (int)sizeof(struct input_event))
        {
            printf("yyy\n");
            perror("\nevtest: error reading");
            return (void *)1;
        }
        
        for (i = 0; i < rd / sizeof(struct input_event); i++)
        {
            if (ev[i].type == EV_KEY && (ev[i].value == 1 || ev[i].value == 2))
            {
                switch (ev[i].code)
                {
                        
                    case KEY_0:
                    {
                        break;
                    }
                    case KEY_1:
                    {
                        break;
                    }
                    case KEY_2:
                    {
                        break;
                    }
                    case KEY_3:
                    {
                        break;
                    }
                    case KEY_4:
                    {
                        break;
                    }
                    case KEY_5:
                    {
                        break;
                    }
                    case KEY_6:
                    {
                        break;
                    }
                    case KEY_7:
                    {
                        break;
                    }
                    case KEY_8:
                    {
                        break;
                    }
                    case KEY_9:
                    {
                        break;
                    }
                    case KEY_NUMERIC_STAR:
                    {
                        break;
                    }
                    case KEY_NUMERIC_POUND:
                    {
                        break;
                    }
                    case KEY_UP:
                    {
                        uint8_t  newBrightness = MIN(matrix->brightness() + BRIGHTNESS_INCREMENT, 100);
                        matrix->SetBrightness(newBrightness);
                        fprintf(stderr, "Set brighness to %d\n", newBrightness);
                        snprintf(gl_infoMessage, INFO_MESSAGE_LENGTH, "Luminosité %d%%", newBrightness);
                        scheduleInfoMessage();
                        break;
                    }
                    case KEY_DOWN:
                    {
                        uint8_t  newBrightness = MAX(matrix->brightness() - BRIGHTNESS_INCREMENT, 0);
                        matrix->SetBrightness(newBrightness);
                        fprintf(stderr, "Set brighness to %d\n", newBrightness);
                        snprintf(gl_infoMessage, INFO_MESSAGE_LENGTH, "Luminosité %d%%", newBrightness);
                        scheduleInfoMessage();
                        break;
                    }
                    case KEY_LEFT:
                    {
                        break;
                    }
                    case KEY_RIGHT:
                    {
                        break;
                    }
                    case KEY_OK:
                    {
                        printf("OKOK!\n");
                        break;
                    }
                }
            }
        }
    }
    
    return (void *)0;
}


void *LoadFile(void *inParam)
{
    setThreadPriority(3, (1 << 2));
    
    FileCollection *collection = (FileCollection *)inParam;
    
    //  for (unsigned int i = 0; i < collection->visibleImages; i++)
    while (collection->loadedFiles.size() < collection->visibleImages * 2)
    {
        std::vector<Magick::Image> frames;
        int count = 0;
        int maxTries = 3;
        while (true)
        {
            int randCount = rand() % collection->filePaths.size();
            const char *imagePath = collection->filePaths[randCount];
            try
            {
                fprintf(stderr, "Attempt to load >%s<\n", imagePath);
                readImages(&frames, imagePath);
                
                LoadedFile  *loadedFile = new LoadedFile();
                
                
                
                loadedFile->filename = imagePath;
                loadedFile->is_multi_frame = frames.size() > 1;
                loadedFile->frameCount = frames.size();
                loadedFile->currentFrameID = -1;
                loadedFile->nextFrameTime = GetTimeInMillis();
                collection->loadedFiles.push_back(loadedFile);
                
                break;
            }
            catch (std::exception &e)
            {
                fprintf(stderr, "Failed to load file: %s", e.what());
                if (++count == maxTries)
                    
                {
                    fprintf(stderr, "Too many errors when loading file: %s", e.what());
                    pthread_exit((void *)1);
                }
            }
        }
        if (frames.size() == 0)
        {
            fprintf(stderr, "No image found.");
            pthread_exit((void *)1);
        }
        if (frames.size() > 1)
        {
            Magick::coalesceImages(&(collection->loadedFiles.back()->frames), frames.begin(), frames.end());
        }
        else
        {
            //            &((*loadedFile)[0].frames)->push_back(loadedFile[0]);   // just a single still image.
        }
    }
    
    pthread_exit((void *)0);
}

void blitzFrameInCanvas(RGBMatrix *matrix, FrameCanvas *offscreen_canvas, Magick::Image &img, unsigned int position, ScreenMode screenMode)
{
    if (screenMode == FullScreen)
    {
        img.scale(Magick::Geometry(matrix->width(), matrix->height()));
    }
    int x_offset = position % 2 == 0 ? 0 : matrix->width() / 2;
    
    int y_offset = position <= 1 ? 0 : matrix->height() / 2;
    for (size_t y = 0; y < img.rows(); ++y)
    {
        for (size_t x = 0; x < img.columns(); ++x)
        {
            const Magick::Color &c = img.pixelColor(x, y);
            if (c.alphaQuantum() < 256)
            {
                offscreen_canvas->SetPixel(x + x_offset, y + y_offset,
                                           ScaleQuantumToChar(c.redQuantum()),
                                           ScaleQuantumToChar(c.greenQuantum()),
                                           ScaleQuantumToChar(c.blueQuantum()));
            }
        }
    }
}

void drawCross(RGBMatrix *matrix, FrameCanvas *offscreen_canvas)
{
    for (int i = 0; i < matrix->width(); i++)
    {
        offscreen_canvas->SetPixel(i, matrix->height() / 2 - 1, 0, 0, 0);
        offscreen_canvas->SetPixel(i, matrix->height() / 2, 0, 0, 0);
    }
    for (int i = 0; i < matrix->height(); i++)
    {
        offscreen_canvas->SetPixel(matrix->width() / 2 - 1, i, 0, 0, 0);
        offscreen_canvas->SetPixel(matrix->width() / 2, i, 0, 0, 0);
    }
}

void displayLoop(RGBMatrix *matrix)
{
    FrameCanvas *offscreen_canvas = matrix->CreateFrameCanvas();
    pthread_t workerThread = 0;
    pthread_t remoteControlThread = 0;
    bool  shouldChangeCollection = false;
    Sequence  *seq = gl_sequences.front();
    fprintf(stderr, "%d collections available\n", seq->collections.size());
    
    int frameCount = 0;
    
    std::vector<LoadedFile *> *currentImages;
    
    int ret = pthread_create(&remoteControlThread, NULL, MonitorIRRemote, NULL);
    if (ret)
    {
        printf("Failed to create Remote control thread\n");
    }
    
    Font statusFont;
    if (!statusFont.LoadFont("../fonts/10x20.bdf")) {
        fprintf(stderr, "Couldn't load font \n");
        exit(1);
    }
    
    while (!interrupt_received)
    {
        
        tmillis_t frame_start = GetTimeInMillis();
        
        if (workerThread == 0 && seq->nextCollection()->loadedFiles.size() < seq->nextCollection()->visibleImages * 2)
        {
            fprintf(stderr, " => %d loaded files in collection %s, need %d\n", seq->nextCollection()->loadedFiles.size(), seq->nextCollection()->regex, seq->nextCollection()->visibleImages);
            int ret = pthread_create(&workerThread, NULL, LoadFile, (void *)(seq->nextCollection()));
            if (ret)
            {
                printf("Failed to create worker thread\n");
            }
        }
        
        if (workerThread)
        {
            void *threadRetval = NULL;
            if (0 == pthread_tryjoin_np(workerThread, &threadRetval))
            {
                fprintf(stderr, "\033[0;31mWorker thread has finished\033[0m with value %d\n", (int)threadRetval);
                workerThread = 0;
                if (seq->currentCollection() == NULL) {
                    seq->forwardCollection();
                    currentImages = &(seq->currentCollection()->loadedFiles);
                    seq->currentCollection()->displayStartTime = GetTimeInMillis();
                }
            }
        }
        if (shouldChangeCollection == true && seq->nextCollection()->loadedFiles.size() >= seq->nextCollection()->visibleImages * 2)
        {
            fprintf(stderr, "\033[0;31mClear loaded files, move to next that contains %d images\033\n",  seq->nextCollection()->loadedFiles.size());
            seq->currentCollection()->loadedFiles.erase(seq->currentCollection()->loadedFiles.begin(), seq->currentCollection()->loadedFiles.begin() + seq->currentCollection()->visibleImages);
            seq->forwardCollection();
            currentImages = &(seq->currentCollection()->loadedFiles);
            seq->currentCollection()->displayStartTime = GetTimeInMillis();
            
        }
        
        shouldChangeCollection = false;
        if (seq->currentCollection() != NULL)
        {
            bool shouldChangeDisplay = false;
            
            for (unsigned short i = 0; i < seq->currentCollection()->visibleImages; i++)
            {
                bool needFrameChange = GetTimeInMillis() > (*currentImages)[i]->nextFrameTime;
                if (needFrameChange)
                {
                    shouldChangeDisplay = true;
                    break;
                }
            }
            
            if (shouldChangeDisplay)
            {
                if (seq->currentCollection()->displayDuration > 0 &&  GetTimeInMillis() - seq->currentCollection()->displayStartTime > (seq->currentCollection()->displayDuration * 1000))
                {
                    fprintf(stderr, "\033[0;34mTime is up for current collection, should move to next %d\033", 42);
                    shouldChangeCollection = true;
                }
                
                for (unsigned short i = 0; i < seq->currentCollection()->visibleImages; i++)
                {
                    bool needFrameChange = GetTimeInMillis() > (*currentImages)[i]->nextFrameTime;
                    if (needFrameChange)
                    {
                        if ((*currentImages)[i]->currentFrameID == -1)
                        {
                            (*currentImages)[i]->currentFrameID = 0;
                        } else {
                            (*currentImages)[i]->currentFrameID = ((*currentImages)[i]->currentFrameID + 1) % (*currentImages)[i]->frames.size();
                            if ((*currentImages)[i]->currentFrameID == 0 && seq->currentCollection()->displayDuration == 0) {
                                fprintf(stderr, "\033[0;34mCurrent animation is FINISHED, should move to next collection %d\033", 42); // simplifier: faire ce code après les affichage pour se débarasser du shouldChangeCollection et forweard() après les blitz a l'écran
                                shouldChangeCollection = true;
                            }
                        }
                    }
                    
                    Magick::Image &img = (*currentImages)[i]->frames[(*currentImages)[i]->currentFrameID];
                    if (needFrameChange)
                    {
                        int64_t delay_time_us;
                        
                        if ((*currentImages)[i]->is_multi_frame)
                        {
                            delay_time_us = img.animationDelay() * 1000; // unit in 1/100s
                        }
                        else
                        {
                            delay_time_us = 5 * 1000; // single image.
                        }
                        if (delay_time_us <= 0)
                        {
                            delay_time_us = 100 * 1000; // 1/10sec
                        }
                        (*currentImages)[i]->nextFrameTime = GetTimeInMillis() + delay_time_us / 100.0;
                    }
                    //              fprintf(stderr, "blitz %s\n", currentImages[i].filename);
                    blitzFrameInCanvas(matrix, offscreen_canvas, img, i, seq->currentCollection()->screenMode);
                }
                if (seq->currentCollection()->screenMode == Cross)
                {
                    drawCross(matrix, offscreen_canvas);
                }
                
                if (time(0) < gl_infotimeout)
                {
                    rgb_matrix::Color blackColor = {.r = 0, .g = 0, .b = 0};
                    
                    DrawText(offscreen_canvas, statusFont,
                             0, 0 + statusFont.baseline(),
                             {.r = 255, .g = 255, .b = 255}, &blackColor,
                             gl_infoMessage, 0);
                    
                }
                
                offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
            }
        }
        
        tmillis_t ellapsedTime = GetTimeInMillis() - frame_start;
        tmillis_t next_frame = frame_start + (1000.0 / FRAME_PER_SECOND) - ellapsedTime;
        SleepMillis(next_frame - frame_start);
        
        frameCount++;
    }
}

static bool LoadImageAndScale(const char *filename,
                              int target_width, int target_height,
                              bool fill_width, bool fill_height,
                              std::vector<Magick::Image> *result,
                              std::string *err_msg)
{
    std::vector<Magick::Image> frames;
    try
    {
        readImages(&frames, filename);
    }
    catch (std::exception &e)
    {
        if (e.what())
            *err_msg = e.what();
        return false;
    }
    if (frames.size() == 0)
    {
        fprintf(stderr, "No image found.");
        return false;
    }
    
    // Put together the animation from single frames. GIFs can have nasty
    // disposal modes, but they are handled nicely by coalesceImages()
    if (frames.size() > 1)
    {
        Magick::coalesceImages(result, frames.begin(), frames.end());
    }
    else
    {
        result->push_back(frames[0]); // just a single still image.
    }
    
    return true;
}

int main(int argc, char *argv[])
{
    srand(time(0));
    Magick::InitializeMagick(*argv);
    RGBMatrix::Options matrix_options;
    rgb_matrix::RuntimeOptions runtime_opt;
    if (!rgb_matrix::ParseOptionsFromFlags(&argc, &argv, &matrix_options, &runtime_opt))
    {
        return usage(argv[0]);
    }
    int displayDuration = 10;
    
    const char *stream_output = NULL;
    char *gifDirectory = NULL;
    int opt;
    while ((opt = getopt(argc, argv, "w:t:l:c:P:hO:d:c:f:s:")) != -1)
    {
        switch (opt)
        {
            case 'w':
                displayDuration = atoi(optarg);
                break;
            case 'd':
                gifDirectory = optarg;
                break;
            case 'c':
            {
                FileCollection *newCollection = new FileCollection();
                newCollection->screenMode = Cross;
                newCollection->displayDuration = displayDuration;
                newCollection->regex = optarg;
                newCollection->loadedFiles = std::vector<LoadedFile *>();
                newCollection->visibleImages = 4;
                gl_sequences.back()->collections.push_back(newCollection);
            }
                break;
            case 'f':
            {
                FileCollection *newCollection = new FileCollection();
                newCollection->screenMode = FullScreen;
                newCollection->displayDuration = displayDuration;
                newCollection->regex = optarg;
                newCollection->loadedFiles = std::vector<LoadedFile *>();
                newCollection->visibleImages = 1;
                gl_sequences.back()->collections.push_back(newCollection);
            }
                break;
            case 'P':
                matrix_options.parallel = atoi(optarg);
                break;
            case 's':
            {
                Sequence *newSequence = new Sequence();
                newSequence->name = optarg;
                gl_sequences.push_back(newSequence);
            }
                break;
            case 'h':
            default:
                return usage(argv[0]);
        }
    }
    
    DIR *gifDir = opendir(gifDirectory);
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
        
        if (isValidDirent(entry))
        {
            int mallocSize = sizeof(char) * (strlen(gifDirectory) + strlen(entry->d_name) + 2);
            char *filePath = (char *)malloc(mallocSize);
            sprintf(filePath, "%s/%s", gifDirectory, entry->d_name);
            gl_filenames.push_back(filePath);
        }
        errno = 0;
    }
    
    fprintf(stderr, "%d valid files\n", gl_filenames.size());
#ifdef MEGA_VERBOSE
    for (unsigned int i = 0; i < gl_filenames.size(); i++)
    {
        fprintf(stderr, ">%s<\n", gl_filenames[i]);
    }
#endif
    
    for ( auto &sequence : gl_sequences ) {
        
       sequence->loadCollections(gl_filenames);
         sequence->printContent();
    }
    
    runtime_opt.do_gpio_init = (stream_output == NULL);
    matrix = CreateMatrixFromOptions(matrix_options, runtime_opt);
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
