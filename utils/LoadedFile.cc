#include "LoadedFile.hh"
#include <Magick++.h>
#include <magick/image.h>

LoadedFile::LoadedFile() {
is_multi_frame = 0;
currentFrameID = 0;
nextFrameTime = -distant_future;
//    LoadedFile(): is_multi_frame(0), currentFrameID(0), nextFrameTime(-distant_future){};

}

LoadedFile::~LoadedFile() 
{
    fprintf(stderr, "Destroy loaded file %s with %d frames\n", filename, frames.size());
    for (auto &frame : frames) {
//        delete frame;
DestroyImage(&frame);

    }
    frames.clear();
}