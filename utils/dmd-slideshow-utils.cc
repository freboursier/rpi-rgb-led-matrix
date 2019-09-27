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