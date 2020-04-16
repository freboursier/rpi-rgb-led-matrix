
#ifndef LOADED_FILE_H
#define LOADED_FILE_H
#include <wand/magick_wand.h>
#include "dmd-slideshow.hh"

class    LoadedFile {
    public:
    LoadedFile();
    ~LoadedFile();

    const char *filename;
    MagickWand  *wand;
    tmillis_t    nextFrameTime;
};

#endif