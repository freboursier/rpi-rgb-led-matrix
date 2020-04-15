
#ifndef LOADED_FILE_H
#define LOADED_FILE_H
#include <Magick++.h>
#include <magick/image.h>
#include <vector>
#include "dmd-slideshow.h"

class    LoadedFile {
    public:
    LoadedFile();
    ~LoadedFile();
    const char *filename;
    std::vector<Magick::Image> frames;
    bool is_multi_frame;
    int    currentFrameID;
    unsigned int frameCount;
    tmillis_t    nextFrameTime;
};

#endif