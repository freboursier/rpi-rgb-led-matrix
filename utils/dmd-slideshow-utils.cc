#include "dmd-slideshow-utils.hh"
#include <string.h>
#include <stdio.h>
#include "led-matrix.h"

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