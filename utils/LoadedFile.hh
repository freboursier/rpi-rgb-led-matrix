
#ifndef LOADED_FILE_H
#define LOADED_FILE_H
#include <wand/magick_wand.h>
//#include "dmd-slideshow.hh"
#include "slideshow_types.hh"

class    LoadedFile {
    private:
    const char *_filename;
    MagickWand  *_wand;

    public:
    LoadedFile(const char *filename, MagickWand *coalescedWand);
    ~LoadedFile();

    MagickWand  *wand();
    const char *filename();
    tmillis_t    nextFrameTime;
};

#endif