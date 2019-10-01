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
// $ make led-image-viewer

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

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include <Magick++.h>
#include <magick/image.h>

#include    "dmd-slideshow-utils.hh"

//#define	MEGA_VERBOSE	

#define		IMAGE_DISPLAY_DURATION	2.0
#define		FRAME_PER_SECOND		5.0

std::vector<const char *> gl_filenames;

using rgb_matrix::GPIO;
using rgb_matrix::Canvas;
using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;
using rgb_matrix::StreamReader;

typedef int64_t tmillis_t;
static const tmillis_t distant_future = (1LL<<40); // that is a while.

struct ImageParams {
    ImageParams() : anim_duration_ms(distant_future), wait_ms(1500),
    anim_delay_ms(-1), loops(-1), vsync_multiple(1) {}
    tmillis_t anim_duration_ms;  // If this is an animation, duration to show.
    tmillis_t wait_ms;           // Regular image: duration to show.
    tmillis_t anim_delay_ms;     // Animation delay override.
    int loops;
    int vsync_multiple;
};

struct	LoadedFile {
	LoadedFile(): is_multi_frame(0), currentFrameID(0), nextFrameTime(-distant_future){};
    const char *filename;
    std::vector<Magick::Image> frames;
	bool is_multi_frame;
	unsigned int	currentFrameID;
	unsigned int frameCount;
//	 int64_t delay_time_us;
	tmillis_t	nextFrameTime;
};

struct FileInfo {
    ImageParams params;      // Each file might have specific timing settings
    bool is_multi_frame;
};

volatile bool interrupt_received = false;
static void InterruptHandler(int signo) {
    interrupt_received = true;
}

static tmillis_t GetTimeInMillis() {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

static void SleepMillis(tmillis_t milli_seconds) {
    if (milli_seconds <= 0) return;
    struct timespec ts;
    ts.tv_sec = milli_seconds / 1000;
    ts.tv_nsec = (milli_seconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

bool	initialLoadDone = false;

void	setThreadPriority(int priority, uint32_t affinity_mask)
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

void  *LoadFile(void *inParam)
{					
	fprintf(stderr, "Start worker thread\n");
	setThreadPriority(1, (1<<2));
    std::vector<LoadedFile> *loadedFiles = (std::vector<LoadedFile> *)inParam;
	
	for (unsigned int i = 0; i < loadedFiles->size(); i++)
	{
	    std::vector<Magick::Image> frames;
		int	randCount = rand() % gl_filenames.size();
		fprintf(stderr, "Available image count: %d, bet on %d\n", gl_filenames.size(), randCount);
		const char	*imagePath = gl_filenames[randCount];
	    try {
			fprintf(stderr, "Attempt to load >%s<\n", imagePath);
	        readImages(&frames, imagePath);
	    } catch (std::exception& e) {
	        fprintf(stderr, "Exception: %s", e.what());
	        pthread_exit((void *)1);
	    }
	    if (frames.size() == 0) {
	        fprintf(stderr, "No image found.");
	        pthread_exit((void *)1);
	    }
		(*loadedFiles)[i].is_multi_frame = frames.size() > 1;
	    (*loadedFiles)[i].frameCount = frames.size();
		(*loadedFiles)[i].nextFrameTime = GetTimeInMillis();
	    // Put together the animation from single frames. GIFs can have nasty
	    // disposal modes, but they are handled nicely by coalesceImages()
	    if (frames.size() > 1) {
	        Magick::coalesceImages(&((*loadedFiles)[i].frames), frames.begin(), frames.end());
	    } else {
	        //			&((*loadedFile)[0].frames)->push_back(loadedFile[0]);   // just a single still image.
	    }
	}

    pthread_exit((void *)0);
}

void	blitzFrameInCanvas(FrameCanvas *offscreen_canvas, const Magick::Image &img, unsigned int position)
{
	int	x_offset = position % 2 == 0 ? 0 : 128;
	
	int	y_offset = position <= 1 ? 0 : 32;
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

void	drawCross(FrameCanvas *offscreen_canvas)
{
	for (int i = 0; i < 256; i++)
	{
        offscreen_canvas->SetPixel(i, 31,
                          ScaleQuantumToChar(0),
                          ScaleQuantumToChar(0),
                          ScaleQuantumToChar(0));
				          offscreen_canvas->SetPixel(i, 32,
				                            ScaleQuantumToChar(0),
				                            ScaleQuantumToChar(0),
				                            ScaleQuantumToChar(0));
						  
	}
}
	
void		displayLoop(std::vector<const char *> filenames, RGBMatrix *matrix)
{
    FrameCanvas *offscreen_canvas = matrix->CreateFrameCanvas();
    pthread_t workerThread = 0;
    
    int		frameCount = 0;
    std::vector<LoadedFile> loadedFiles(4);
    std::vector<LoadedFile> currentImages(4);
	
    while (!interrupt_received)
    {
        tmillis_t	frame_start = GetTimeInMillis();
        fprintf(stderr, "Start frame %d\n", frameCount);
        
        if (frameCount % 20 == 0 && workerThread == 0)
        {
            int ret = pthread_create(&workerThread, NULL, LoadFile, &loadedFiles);
            if (ret) {
                printf("Failed to create worker thread\n");
            }
        }
        
        if (workerThread) {
            void	*threadRetval = NULL;
            int joinResult = pthread_tryjoin_np(workerThread, &threadRetval);
            if (joinResult == 0) {
                printf("\033[0;31mWorker thread has finished\033[0m with value %d\n", (int)threadRetval);
				std::vector<LoadedFile>  tmp = currentImages;
				currentImages = loadedFiles;
				loadedFiles = tmp;
				
                workerThread = 0;
				initialLoadDone = true;
            }
        }
        
		if (initialLoadDone) {
			for (unsigned int i = 0; i < 4; i++)
			{
				bool	needFrameChange = GetTimeInMillis() > currentImages[i].nextFrameTime;
				fprintf(stderr, "Current time is %lld, nextFrameTime is %lld, => %s\n", GetTimeInMillis(), currentImages[i].nextFrameTime, needFrameChange ? "YES" : "NO");
				if (needFrameChange)
				{
					fprintf(stderr, "Select next frame for file %d\n", i);
					currentImages[i].currentFrameID = (currentImages[i].currentFrameID + 1) % currentImages[i].frames.size();
				}
				
				
				
				
				const Magick::Image &img = currentImages[i].frames[currentImages[i].currentFrameID];
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
					fprintf(stderr, "delay_time_us: %.3lld\n", delay_time_us);
						currentImages[i].nextFrameTime = GetTimeInMillis() + delay_time_us;
						
				}
				
				blitzFrameInCanvas(offscreen_canvas, img, i);
			}
			drawCross(offscreen_canvas);
			offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas, 1);
		}
        tmillis_t	ellapsedTime = GetTimeInMillis() - frame_start;
        tmillis_t	next_frame = frame_start + (1000.0 / FRAME_PER_SECOND) - ellapsedTime;
        SleepMillis(next_frame - frame_start);
        
        frameCount++;
    }
    
}

static void StoreInStream(FileInfo *file,  std::vector<Magick::Image> image_sequence,
                          bool do_center,
                          rgb_matrix::FrameCanvas *scratch,
                          RGBMatrix *matrix) {
    const tmillis_t duration_ms = (file->is_multi_frame
                                   ? file->params.anim_duration_ms
                                   : file->params.wait_ms);
    int loops = file->params.loops;
    const tmillis_t end_time_ms = GetTimeInMillis() + duration_ms;
    const tmillis_t override_anim_delay = file->params.anim_delay_ms;
    for (int k = 0;
         (loops < 0 || k < loops)
         && !interrupt_received
         && GetTimeInMillis() < end_time_ms;
         ++k) {
        //uint32_t delay_us = 0;
        uint	frame_id = 0;
        
        
        while (!interrupt_received && GetTimeInMillis() <= end_time_ms
               /*&& reader.GetNext(offscreen_canvas, &delay_us)*/) {
            
            const Magick::Image &img = image_sequence[frame_id];
            int64_t delay_time_us;
            if (file->is_multi_frame) {
                delay_time_us = img.animationDelay() * 10000; // unit in 1/100s
            } else {
                delay_time_us = file->params.wait_ms * 1000;  // single image.
            }
            if (delay_time_us <= 0)
            {
                delay_time_us = 100 * 1000;  // 1/10sec
            }
            
            const tmillis_t anim_delay_ms =
            override_anim_delay >= 0 ? override_anim_delay : delay_time_us / 1000;
            const tmillis_t start_wait_ms = GetTimeInMillis();
            scratch->Clear();
            const int x_offset = do_center ? (scratch->width() - img.columns()) / 2 : 0;
            const int y_offset = do_center ? (scratch->height() - img.rows()) / 2 : 0;
            for (size_t y = 0; y < img.rows(); ++y) {
                for (size_t x = 0; x < img.columns(); ++x) {
                    const Magick::Color &c = img.pixelColor(x, y);
                    if (c.alphaQuantum() < 256) {
                        scratch->SetPixel(x + x_offset, y + y_offset,
                                          ScaleQuantumToChar(c.redQuantum()),
                                          ScaleQuantumToChar(c.greenQuantum()),
                                          ScaleQuantumToChar(c.blueQuantum()));
                    }
                }
            }
            
            scratch = matrix->SwapOnVSync(scratch, file->params.vsync_multiple);
            const tmillis_t time_already_spent = GetTimeInMillis() - start_wait_ms;
            SleepMillis(anim_delay_ms - time_already_spent);
            frame_id++;
            if (frame_id == image_sequence.size())
            {
                frame_id = 0;
            }
        }
    }
}


// Load still image or animation.
// Scale, so that it fits in "width" and "height" and store in "result".
static bool LoadImageAndScale(const char *filename,
                              int target_width, int target_height,
                              bool fill_width, bool fill_height,
                              std::vector<Magick::Image> *result,
                              std::string *err_msg) {
    std::vector<Magick::Image> frames;
    time_t	start_load = GetTimeInMillis();
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
    /*
     const int img_width = (*result)[0].columns();
     const int img_height = (*result)[0].rows();
     const float width_fraction = (float)target_width / img_width;
     const float height_fraction = (float)target_height / img_height;
     if (fill_width && fill_height) {
     // Scrolling diagonally. Fill as much as we can get in available space.
     // Largest scale fraction determines that.
     const float larger_fraction = (width_fraction > height_fraction)
     ? width_fraction
     : height_fraction;
     target_width = (int) roundf(larger_fraction * img_width);
     target_height = (int) roundf(larger_fraction * img_height);
     }
     else if (fill_height) {
     // Horizontal scrolling: Make things fit in vertical space.
     // While the height constraint stays the same, we can expand to full
     // width as we scroll along that axis.
     target_width = (int) roundf(height_fraction * img_width);
     }
     else if (fill_width) {
     // dito, vertical. Make things fit in horizontal space.
     target_height = (int) roundf(width_fraction * img_height);
     }
     fprintf(stdout, "initigal GIF loading took %.3fs; \n",
     (GetTimeInMillis() - start_load) / 1000.0);
     
     time_t	start_scale = GetTimeInMillis();
     for (size_t i = 0; i < result->size(); ++i) {
     (*result)[i].scale(Magick::Geometry(target_width, target_height));
     }
     fprintf(stdout, "GIF scaling took %.3fs; \n",
     (GetTimeInMillis() - start_scale) / 1000.0);
     
     fprintf(stdout, "GIF loading took %.3fs; now: Display.\n",
     (GetTimeInMillis() - start_load) / 1000.0);
     */
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
    
    bool do_center = false;
    
    // We remember ImageParams for each image, which will change whenever
    // there is a flag modifying them. This map keeps track of filenames
    // and their image params (also for unrelated elements of argv[], but doesn't
    // matter).
    // We map the pointer instad of the string of the argv parameter so that
    // we can have two times the same image on the commandline list with different
    // parameters.
    std::map<const void *, struct ImageParams> filename_params;
    
    // Set defaults.
    ImageParams img_param;
    for (int i = 0; i < argc; ++i) {
        filename_params[argv[i]] = img_param;
    }
    
    const char *stream_output = NULL;
    char *gifDirectory = NULL;
    int opt;
    while ((opt = getopt(argc, argv, "w:t:l:c:P:hCO:d:")) != -1) {
        switch (opt) {
            case 'w':
                img_param.wait_ms = roundf(atof(optarg) * 1000.0f);
                break;
            case 't':
                img_param.anim_duration_ms = roundf(atof(optarg) * 1000.0f);
                break;
            case 'd':
                gifDirectory = optarg;
                break;
            case 'l':
                img_param.loops = atoi(optarg);
                break;
            case 'C':
                do_center = true;
                break;
            case 'P':
                matrix_options.parallel = atoi(optarg);
                break;
            case 'h':
            default:
                return usage(argv[0]);
        }
        
        // Starting from the current file, set all the remaining files to
        // the latest change.
        for (int i = optind; i < argc; ++i) {
            filename_params[argv[i]] = img_param;
        }
    }
    
    
    DIR	*gifDir = opendir(gifDirectory);
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
        //char *filePath = strcat(gifDirectory,  entry->d_name);
        
		if (isValidDirent(entry)) {
			 int	mallocSize = sizeof(char) * (strlen(gifDirectory) + strlen(entry->d_name) + 2);
	         char *filePath = ( char *)malloc(mallocSize);
			 sprintf(filePath, "%s/%s", gifDirectory, entry->d_name);
			 gl_filenames.push_back(filePath);
		}
        errno = 0;
    }
   
   fprintf(stderr, "%d valid files\n", gl_filenames.size());
#ifdef	MEGA_VERBOSE
   for (unsigned int i = 0; i < gl_filenames.size(); i++)
   {
	   fprintf(stderr, ">%s<\n", gl_filenames[i]);
   }
#endif

    // Prepare matrix
    runtime_opt.do_gpio_init = (stream_output == NULL);
    RGBMatrix *matrix = CreateMatrixFromOptions(matrix_options, runtime_opt);
    if (matrix == NULL)
	{
		fprintf(stderr, "Fatal: Failed to create matrix\n");
        return 1;
	}
    
    
    
    printf("Size: %dx%d. Hardware gpio mapping: %s\n",
           matrix->width(), matrix->height(), matrix_options.hardware_mapping);
    
    // These parameters are needed once we do scrolling.
    const bool fill_width = false;
    const bool fill_height = false;
    
    //    const tmillis_t start_load = GetTimeInMillis();
    // fprintf(stderr, "Loading %d files...\n", argc - optind);
    // Preparing all the images beforehand as the Pi might be too slow to
    // be quickly switching between these. So preprocess.
    
    
    // for (int imgarg = optind; imgarg < argc; ++imgarg) {
    //     const char *filename = argv[imgarg];
    //     gl_filenames.push_back(filename);
    // }
    //
    
    fprintf(stderr, "%d available images\n", gl_filenames.size());
    
    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);
    
    displayLoop(gl_filenames, matrix);
    
    
    //	exit(0);
    // std::vector<FileInfo*> file_imgs;
    // for (int imgarg = optind; imgarg < argc; ++imgarg) {
    //     const char *filename = argv[imgarg];
    //     FileInfo *file_info = NULL;
    //
    //     std::string err_msg;
    //     std::vector<Magick::Image> image_sequence;
    //     if (LoadImageAndScale(filename, matrix->width(), matrix->height(),
    //                           fill_width, fill_height, &image_sequence, &err_msg)) {
    //         file_info = new FileInfo();
    //         file_info->params = filename_params[filename];
    //         file_info->is_multi_frame = image_sequence.size() > 1;
    //
    //         StoreInStream(file_info, image_sequence, do_center, offscreen_canvas, matrix);
    //     }
    //     if (file_info) {
    //         file_imgs.push_back(file_info);
    //     } else {
    //         fprintf(stderr, "%s skipped: Unable to open (%s)\n",
    //                 filename, err_msg.c_str());
    //     }
    // }
    //
    // signal(SIGTERM, InterruptHandler);
    // signal(SIGINT, InterruptHandler);
    
    // Animation finished. Shut down the RGB matrix.
    matrix->Clear();
    delete matrix;
    
    // Leaking the FileInfos, but don't care at program end.
    return 0;
}

