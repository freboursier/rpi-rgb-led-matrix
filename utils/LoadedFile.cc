#include "LoadedFile.hh"
// #include <Magick++.h>
// #include <magick/image.h>

LoadedFile::LoadedFile() {
  nextFrameTime = -distant_future;
}

LoadedFile::~LoadedFile() {
  DestroyMagickWand(wand);
}